#include "anim.h"
#include "graph.h"
#include "graph-io.h"
#include "draw.h"
#include "module.h"
#include "cycles.h"
#include "modules/api.h"
#include "modules/localsize.h"
#include "core/log.h"
#include "qvk/qvk.h"
#include "graph-print.h"
#ifdef DEBUG_MARKERS
#include "db/stringpool.h"
#endif
#include "cycles.h"
#include "graph-run-modules.h"
#include "graph-run-nodes-allocate.h"
#include "graph-run-nodes-upload.h"
#include "graph-run-nodes-record-cmd.h"
#include "graph-run-nodes-download.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gui/gui.h"

static VkResult dt_graph_init_mipmap(dt_graph_t *g);
static void dt_graph_cleanup_mipmap(dt_graph_t *g);

void
dt_graph_init(dt_graph_t *g, qvk_queue_name_t qname)
{
  memset(g, 0, sizeof(*g));
#ifdef DEBUG_MARKERS
  dt_stringpool_init(&g->debug_markers, 2000, 20);
#endif

  g->frame_cnt = 1;
  g->queue_name = qname;

  // allocate module and node buffers:
  g->max_modules = 100;
  g->module = calloc(sizeof(dt_module_t), g->max_modules);
  g->max_nodes = 4000;
  g->node = calloc(sizeof(dt_node_t), g->max_nodes);
  for(int i=0;i<32;i++) dt_vkalloc_init(&g->heap[i], 16000, ((uint64_t)1)<<40); // bytesize doesn't matter
  g->params_max = 16u<<20;
  g->params_end = 0;
  g->params_pool = calloc(sizeof(uint8_t), g->params_max);

  g->conn_image_max = 30*2*2000; // connector images for a lot of connectors
  g->conn_image_end = 0;
  g->conn_image_pool = calloc(sizeof(dt_connector_image_t), g->conn_image_max);

  VkCommandPoolCreateInfo cmd_pool_create_info = {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = qvk.queue[qvk.qid[g->queue_name]].family,
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };
  QVK(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, &g->command_pool));

  VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = g->command_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 2,
  };
  QVK(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, g->command_buffer));

  // when the compute queue belongs to a compute-only family we need a second,
  // graphics-family command pool to fall back to when s_node_graphics nodes
  // (draw, overlay) are present on the graph.
  if(qvk.queue_family_compute != qvk.queue_family_graphics)
  {
    cmd_pool_create_info.queueFamilyIndex = qvk.queue_family_graphics;
    QVK(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, &g->command_pool_gfx));
    cmd_buf_alloc_info.commandPool = g->command_pool_gfx;
    QVK(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, g->command_buffer_gfx));
  }
  VkSemaphoreTypeCreateInfo timelineCreateInfo = {
    .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    .pNext         = NULL,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    .initialValue  = 0,
  };
  VkSemaphoreCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &timelineCreateInfo,
    .flags = 0,
  };
  vkCreateSemaphore(qvk.device, &createInfo, NULL, &g->semaphore_display);
  vkCreateSemaphore(qvk.device, &createInfo, NULL, &g->semaphore_process);
  for(int i=0;i<2;i++)
  {
    g->query[i].max = 2000;
    g->query[i].cnt = 0;
    VkQueryPoolCreateInfo query_pool_info = {
      .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType  = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = g->query[i].max,
    };
    QVK(vkCreateQueryPool(qvk.device, &query_pool_info, NULL, &g->query[i].pool));
    g->query[i].pool_results = malloc(sizeof(uint64_t)*g->query[i].max);
    g->query[i].name   = malloc(sizeof(dt_token_t)*g->query[i].max);
    g->query[i].kernel = malloc(sizeof(dt_token_t)*g->query[i].max);
  }

  g->lod_scale = 1;
  g->active_module = -1;
  dt_graph_init_mipmap(g);
}

// Destroy per-image/per-graph-structure Vulkan resources: connector images,
// staging buffers, pipelines, layouts, framebuffers.  Does NOT touch the
// stable Vulkan objects (command pool, semaphores, query pools, device memory).
static void
graph_destroy_per_image_resources(dt_graph_t *g)
{
  for(int i=0;i<g->num_nodes;i++)
  {
    for(int j=0;j<g->node[i].num_connectors;j++)
    {
      dt_connector_t *c = g->node[i].connector+j;
      if(c->flags & s_conn_dynamic_array)
      { // clear potential double images for multi-frame dsets
        for(int k=0;k<MAX(1,c->array_length);k++)
        { // avoid error message in case image does not exist:
          dt_connector_image_t *img0 = (g->node[i].conn_image[j] != -1) ? dt_graph_connector_image(g, i, j, k, 0) : 0;
          dt_connector_image_t *img1 = (g->node[i].conn_image[j] != -1) ? dt_graph_connector_image(g, i, j, k, 1) : 0;
          if(!img0 || !img1) continue;
          if(img0->buffer == img1->buffer && img0->image == img1->image)
          { // it's enough to clean up one replicant, the rest will shut down cleanly later:
            *img0 = (dt_connector_image_t){0};
          }
        }
      }
    }
  }
  for(int i=0;i<g->conn_image_end;i++)
  {
    if(g->conn_image_pool[i].buffer)     vkDestroyBuffer   (qvk.device, g->conn_image_pool[i].buffer,     VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, g->conn_image_pool[i].image_view, VK_NULL_HANDLE);
    for(int m=0;m<14;m++) if(g->conn_image_pool[i].mipmap_views[m]) vkDestroyImageView(qvk.device, g->conn_image_pool[i].mipmap_views[m], VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image)      vkDestroyImage    (qvk.device, g->conn_image_pool[i].image,      VK_NULL_HANDLE);
    g->conn_image_pool[i] = (dt_connector_image_t){0};
  }
  for(int i=0;i<g->num_nodes;i++)
  {
    for(int j=0;j<g->node[i].num_connectors;j++)
    {
      dt_connector_t *c = g->node[i].connector+j;
      if(c->staging[0]) vkDestroyBuffer(qvk.device, c->staging[0], VK_NULL_HANDLE);
      if(c->staging[1]) vkDestroyBuffer(qvk.device, c->staging[1], VK_NULL_HANDLE);
      c->staging[0] = c->staging[1] = 0;
      if(c->array_heap)
      { // free any potential residuals of dynamic allocation
        dt_vkalloc_cleanup(c->array_heap);
        free(c->array_heap);
        c->array_heap = 0;
        c->array_heap_size = 0;
        c->array_mem = 0;
      }
    }
    vkDestroyPipelineLayout     (qvk.device, g->node[i].pipeline_layout,  0);
    vkDestroyPipeline           (qvk.device, g->node[i].pipeline,         0);
    vkDestroyDescriptorSetLayout(qvk.device, g->node[i].dset_layout,      0);
    g->node[i].pipeline_layout  = 0;
    g->node[i].pipeline         = 0;
    g->node[i].dset_layout      = 0;
    dt_raytrace_node_cleanup(g->node + i);
  }
  dt_raytrace_graph_cleanup(g);
}

// Wait for all in-flight GPU work on this graph to complete.
// Returns VK_SUCCESS or VK_ERROR_DEVICE_LOST.
static VkResult
graph_wait_gpu(dt_graph_t *g, const char *caller)
{
  // if no gui is attached, display indices stay at 0, so this is a no-op
  const uint64_t wait_value[] = {
    MAX(g->display_dbuffer[0], g->display_dbuffer[1]),
    MAX(g->process_dbuffer[0], g->process_dbuffer[1]),
  };
  VkSemaphore sem[] = { g->semaphore_display, g->semaphore_process };
  VkSemaphoreWaitInfo wait_info = {
    .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 2,
    .pSemaphores    = sem,
    .pValues        = wait_value,
  };
  VkResult res = vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX);
  if(res != VK_SUCCESS)
  {
    dt_log(s_log_pipe|s_log_err, "[%s] lost device while waiting for semaphores!", caller);
    return res;
  }
#ifdef __APPLE__
  // MoltenVK: semaphore fires before finishQueries() completes; drain queue before freeing pools.
  vkQueueWaitIdle(qvk.queue[qvk.qid[g->queue_name]].queue);
#endif
  return VK_SUCCESS;
}

// Run module so->cleanup() and free keyframes for all loaded modules.
static void
graph_teardown_modules(dt_graph_t *g)
{
  for(int i=0;i<g->num_modules;i++)
  {
    if(g->module[i].name && g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
    free(g->module[i].keyframe);
    g->module[i].keyframe_size = 0;
    g->module[i].keyframe_cnt  = 0;
    g->module[i].keyframe      = 0;
  }
}

void
dt_graph_cleanup(dt_graph_t *g)
{
  if(!g->module) return; // already cleaned up
#ifdef DEBUG_MARKERS
  dt_stringpool_cleanup(&g->debug_markers);
#endif
  g->active_module = -1;
  if(graph_wait_gpu(g, "graph_cleanup") != VK_SUCCESS) return;
  graph_teardown_modules(g);
  for(int i=0;i<32;i++) dt_vkalloc_cleanup(&g->heap[i]);
  graph_destroy_per_image_resources(g);
  vkDestroyDescriptorPool(qvk.device, g->dset_pool, 0);
  vkDestroyDescriptorSetLayout(qvk.device, g->uniform_dset_layout, 0);
  vkDestroyBuffer(qvk.device, g->uniform_buffer, 0);
  g->dset_pool = 0;
  g->uniform_dset_layout = 0;
  g->uniform_buffer = 0;
  for(int i=0;i<32;i++)
  {
    vkFreeMemory(qvk.device, g->vkmem[i], 0);
    g->vkmem[i] = 0;
    g->vkmem_size[i] = 0;
  }
  vkFreeMemory(qvk.device, g->vkmem_uniform, 0);
  g->vkmem_uniform = 0;
  g->vkmem_uniform_size = 0;
  vkDestroySemaphore(qvk.device, g->semaphore_display, 0);
  vkDestroySemaphore(qvk.device, g->semaphore_process, 0);
  g->semaphore_display = 0;
  g->semaphore_process = 0;
  if(g->command_pool != VK_NULL_HANDLE)
    vkFreeCommandBuffers(qvk.device, g->command_pool, 2, g->command_buffer);
  g->command_buffer[0] = g->command_buffer[1] = VK_NULL_HANDLE;
  vkDestroyCommandPool(qvk.device, g->command_pool, 0);
  g->command_pool = 0;
  if(g->command_pool_gfx != VK_NULL_HANDLE)
  {
    vkFreeCommandBuffers(qvk.device, g->command_pool_gfx, 2, g->command_buffer_gfx);
    g->command_buffer_gfx[0] = g->command_buffer_gfx[1] = VK_NULL_HANDLE;
    vkDestroyCommandPool(qvk.device, g->command_pool_gfx, 0);
    g->command_pool_gfx = 0;
  }
  free(g->module);             g->module = 0;
  free(g->node);               g->node = 0;
  free(g->params_pool);        g->params_pool = 0;
  free(g->conn_image_pool);    g->conn_image_pool = 0;
  g->num_modules = g->num_nodes = 0;
  for(int i=0;i<2;i++)
  {
    vkDestroyQueryPool(qvk.device, g->query[i].pool, 0);
    g->query[i].pool = 0;
    free(g->query[i].pool_results); g->query[i].pool_results = 0;
    free(g->query[i].name);         g->query[i].name = 0;
    free(g->query[i].kernel);       g->query[i].kernel = 0;
  }
  dt_graph_cleanup_mipmap(g);
}

static inline void *
read_file(const char *filename, size_t *len)
{
  FILE *f = dt_graph_open_resource(0, 0, filename, "rb");
  if(!f) return 0;
  fseek(f, 0, SEEK_END);
  const size_t filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *file = malloc(filesize+1);

  size_t rd = fread(file, sizeof(char), filesize, f);
  file[filesize] = 0;
  if(rd != filesize)
  {
    free(file);
    file = 0;
    fclose(f);
    return 0;
  }
  if(len) *len = filesize;
  fclose(f);
  return file;
}

VkResult
dt_graph_create_shader_module(
    dt_graph_t     *graph,
    dt_token_t      node,
    dt_token_t      kernel,
    const char     *type,
    VkShaderModule *shader_module)
{
  // create the compute shader stage
  char filename[PATH_MAX+100] = {0};
  snprintf(filename, sizeof(filename), "modules/%"PRItkn"/%"PRItkn".%s.spv",
      dt_token_str(node), dt_token_str(kernel), type);

  size_t len;
  void *data = read_file(filename, &len);
  if(!data) return VK_ERROR_INVALID_EXTERNAL_HANDLE;

  VkShaderModuleCreateInfo sm_info = {
    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode    = data
  };
  QVKR(vkCreateShaderModule(qvk.device, &sm_info, 0, shader_module));
  free(data);
#if 0 // def DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
  char name[100];
  const char *dedup;
  snprintf(name, sizeof(name), "%"PRItkn"_%"PRItkn, dt_token_str(node), dt_token_str(kernel));
  dt_stringpool_get(&graph->debug_markers, name, strlen(name), 0, &dedup);
  VkDebugMarkerObjectNameInfoEXT name_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
    .object = (uint64_t) shader_module,
    .objectType = VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT,
    .pObjectName = dedup,
  };
  qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
  return VK_SUCCESS;
}

void
dt_graph_generate_mipmaps(
    dt_graph_t           *graph,
    int                   rwd,
    int                   rht,
    dt_connector_image_t *img)
{
  if(img->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return;
  VkCommandBuffer cmd_buf = dt_graph_cmd_buf(graph);
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .image = img->image,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
    .subresourceRange.levelCount = 1,
  };
  int wd = img->wd > 0 ? img->wd : rwd;
  int ht = img->ht > 0 ? img->ht : rht;

  // put all mip levels into general layout since we will read/write them with compute:
  barrier.subresourceRange.levelCount = img->mip_levels;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.oldLayout = img->layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd_buf,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
      0, NULL,
      0, NULL,
      1, &barrier);
  barrier.subresourceRange.levelCount = 1;
  int input_wd = wd;
  int input_ht = ht;
  for (uint32_t i = 1; i < img->mip_levels; )
  {
    if(graph->mipmap_use_down && (i + 2) < img->mip_levels &&
        (input_wd >= 8) && (input_ht >= 8) &&
        (input_wd % 8 == 0) && (input_ht % 8 == 0))
    {
      vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, graph->mipmap_down_pipeline);
      VkDescriptorImageInfo img_in = {
        .sampler = qvk.tex_sampler,
        .imageView = img->mipmap_views[i - 1],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkDescriptorImageInfo img_gbuf = {
        .sampler = qvk.tex_sampler,
        .imageView = img->mipmap_views[i - 1],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkDescriptorImageInfo img_out2 = {
        .sampler = VK_NULL_HANDLE,
        .imageView = img->mipmap_views[i],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkDescriptorImageInfo img_out4 = {
        .sampler = VK_NULL_HANDLE,
        .imageView = img->mipmap_views[i + 1],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkDescriptorImageInfo img_out8 = {
        .sampler = VK_NULL_HANDLE,
        .imageView = img->mipmap_views[i + 2],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkWriteDescriptorSet writes[] = {
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &img_in,
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 1,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &img_gbuf,
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 2,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &img_out2,
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 3,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &img_out4,
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 4,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &img_out8,
        },
      };
      qvkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, graph->mipmap_down_pipeline_layout, 1, 5, writes);

      vkCmdDispatch(cmd_buf, (input_wd + 7) / 8, (input_ht + 7) / 8, 1);

      barrier.subresourceRange.baseMipLevel = i;
      barrier.subresourceRange.levelCount = 3;
      barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(cmd_buf,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
          0, NULL,
          0, NULL,
          1, &barrier);
      barrier.subresourceRange.levelCount = 1;

      if(input_wd > 1) input_wd = MAX(1, input_wd / 8);
      if(input_ht > 1) input_ht = MAX(1, input_ht / 8);
      i += 3;
    }
    else
    {
      vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, graph->mipmap_pipeline);
      VkDescriptorImageInfo img_in = {
        .sampler = qvk.tex_sampler,
        .imageView = img->mipmap_views[i - 1],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkDescriptorImageInfo img_out = {
        .sampler = VK_NULL_HANDLE,
        .imageView = img->mipmap_views[i],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
      VkWriteDescriptorSet writes[] = {
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &img_in,
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 1,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &img_out,
        },
      };
      qvkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, graph->mipmap_pipeline_layout, 0, 2, writes);

      int dwd = input_wd > 1 ? input_wd / 2 : 1;
      int dht = input_ht > 1 ? input_ht / 2 : 1;
      vkCmdDispatch(cmd_buf, (dwd + 7) / 8, (dht + 7) / 8, 1);

      barrier.subresourceRange.baseMipLevel = i;
      barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(cmd_buf,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
          0, NULL,
          0, NULL,
          1, &barrier);

      if(input_wd > 1) input_wd /= 2;
      if(input_ht > 1) input_ht /= 2;
      i += 1;
    }
  }

  barrier.subresourceRange.levelCount = img->mip_levels;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd_buf,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
      0, NULL,
      0, NULL,
      1, &barrier);
  img->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static VkResult dt_graph_init_mipmap(dt_graph_t *g)
{
  VkShaderModule shader_module;
  QVKR(dt_graph_create_shader_module(g, dt_token("mipmap"), dt_token("main"), "comp", &shader_module));

  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    },
    {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    }
  };

  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
    .bindingCount = 2,
    .pBindings = bindings,
  };
  QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, NULL, &g->mipmap_dset_layout));

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &g->mipmap_dset_layout,
  };
  QVKR(vkCreatePipelineLayout(qvk.device, &pipeline_layout_info, NULL, &g->mipmap_pipeline_layout));

  VkComputePipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = shader_module,
      .pName = "main",
    },
    .layout = g->mipmap_pipeline_layout,
  };
  QVKR(vkCreateComputePipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &g->mipmap_pipeline));

  vkDestroyShaderModule(qvk.device, shader_module, NULL);

  g->mipmap_use_down =
    (qvk.subgroup_ops & VK_SUBGROUP_FEATURE_CLUSTERED_BIT) &&
    (qvk.subgroup_ops & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) &&
    (qvk.subgroup_ops & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) &&
    (qvk.subgroup_size_control_supported || qvk.subgroup_size == 32);

  if(g->mipmap_use_down)
  {
    VkDescriptorSetLayoutCreateInfo empty_dset_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 0,
      .pBindings = NULL,
    };
    QVKR(vkCreateDescriptorSetLayout(qvk.device, &empty_dset_layout_info, NULL, &g->mipmap_empty_dset_layout));

    VkShaderModule down_shader_module;
    QVKR(dt_graph_create_shader_module(g, dt_token("svgf2"), dt_token("down"), "comp", &down_shader_module));

    VkDescriptorSetLayoutBinding down_bindings[] = {
      {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      {
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      {
        .binding = 4,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
    };

    VkDescriptorSetLayoutCreateInfo down_dset_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
      .bindingCount = 5,
      .pBindings = down_bindings,
    };
    QVKR(vkCreateDescriptorSetLayout(qvk.device, &down_dset_layout_info, NULL, &g->mipmap_down_dset_layout));

    VkDescriptorSetLayout down_set_layouts[] = {
      g->mipmap_empty_dset_layout,
      g->mipmap_down_dset_layout,
    };
    VkPipelineLayoutCreateInfo down_pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = LENGTH(down_set_layouts),
      .pSetLayouts = down_set_layouts,
    };
    QVKR(vkCreatePipelineLayout(qvk.device, &down_pipeline_layout_info, NULL, &g->mipmap_down_pipeline_layout));

    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT down_subgroup = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
      .requiredSubgroupSize = 32,
    };
    VkPipelineShaderStageCreateInfo down_stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = down_shader_module,
      .pName = "main",
    };
    if(qvk.subgroup_size_control_supported)
    {
      down_stage.pNext = &down_subgroup;
    }

    VkComputePipelineCreateInfo down_pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = down_stage,
      .layout = g->mipmap_down_pipeline_layout,
    };
    QVKR(vkCreateComputePipelines(qvk.device, VK_NULL_HANDLE, 1, &down_pipeline_info, NULL, &g->mipmap_down_pipeline));

    vkDestroyShaderModule(qvk.device, down_shader_module, NULL);
  }
  return VK_SUCCESS;
}

static void dt_graph_cleanup_mipmap(dt_graph_t *g)
{
  if(g->mipmap_pipeline) vkDestroyPipeline(qvk.device, g->mipmap_pipeline, NULL);
  if(g->mipmap_pipeline_layout) vkDestroyPipelineLayout(qvk.device, g->mipmap_pipeline_layout, NULL);
  if(g->mipmap_dset_layout) vkDestroyDescriptorSetLayout(qvk.device, g->mipmap_dset_layout, NULL);
  if(g->mipmap_down_pipeline) vkDestroyPipeline(qvk.device, g->mipmap_down_pipeline, NULL);
  if(g->mipmap_down_pipeline_layout) vkDestroyPipelineLayout(qvk.device, g->mipmap_down_pipeline_layout, NULL);
  if(g->mipmap_down_dset_layout) vkDestroyDescriptorSetLayout(qvk.device, g->mipmap_down_dset_layout, NULL);
  if(g->mipmap_empty_dset_layout) vkDestroyDescriptorSetLayout(qvk.device, g->mipmap_empty_dset_layout, NULL);
}

// convenience function for debugging in gdb:
void dt_token_print(dt_token_t t)
{
  fprintf(stderr, "token: %"PRItkn"\n", dt_token_str(t));
}

static inline void
dt_graph_print_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run)
{
  dt_log(s_log_pipe, "graph run db %d %s %s %s %s %s %s %s %s",
      graph->double_buffer,
      run & s_graph_run_roi ? "roi" : "",
      run & s_graph_run_create_nodes ? "nodes" : "",
      run & s_graph_run_alloc ? "alloc" : "",
      run & s_graph_run_record_cmd_buf ? "cmd_buf" : "",
      run & s_graph_run_upload_source ? "upload" : "",
      run & s_graph_run_download_sink ? "download" : "",
      run & s_graph_run_wait_done ? "wait" : "",
      run & s_graph_run_before_active ? "<active" : "");
}

VkResult dt_graph_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run)
{
  dt_graph_print_run(graph, run);
  double clock_beg = dt_time();
  dt_module_flags_t module_flags = 0;
  // double_buffer is initialised to 0 and has to be set from the outside if flipping the double buffer is requested.
  const int buf_curr = graph->double_buffer & 1; // recording this pipeline and writing to this buffer index now
  const int buf_prev = 1-buf_curr;               // waiting for the previous double buffer
  graph->gui_msg = 0;

  // wait for last invocation of our command buffer to finish:
  VkSemaphoreWaitInfo wait_info = {
    .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 1,
    .pSemaphores    = &graph->semaphore_process,
    .pValues        = &graph->process_dbuffer[buf_curr],
  };
  QVKR(vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX));

  { // module scope
    uint32_t modid[100]; // storage for list of modules after tree traversal
    if(sizeof(modid)/sizeof(modid[0]) < graph->num_modules)
    {
      dt_log(s_log_pipe|s_log_err, "too many modules in graph!");
      return VK_INCOMPLETE;
    }
    QVKR(dt_graph_run_modules(graph, &run, modid, &module_flags));
  } // end scope, done with modules

  // if no more action than generating the output roi was requested, exit now:
  if(run < s_graph_run_create_nodes<<1) return VK_SUCCESS;

  { // node scope
    int cnt = 0;
    dt_node_t *const arr = graph->node;
    const int arr_cnt = graph->num_nodes;
    uint32_t nodeid[2000];
    if(sizeof(nodeid)/sizeof(nodeid[0]) < graph->num_nodes)
    {
      dt_log(s_log_pipe|s_log_err, "too many nodes in graph!");
      return VK_INCOMPLETE;
    }
#define TRAVERSE_POST {\
    module_flags |= arr[curr].flags;\
    if(arr[curr].force_upload) module_flags |= s_module_request_read_source;\
    nodeid[cnt++] = curr;\
  }
#include "graph-traverse.inc"

    if(cnt == 0)
    {
      dt_log(s_log_pipe|s_log_err, "no nodes created!");
      return VK_INCOMPLETE;
    }

    for(int i=0;i<cnt;i++)
    {
      const dt_node_t *node = graph->node+nodeid[i];
      for(int c=0;c<node->num_connectors;c++)
      {
        if(dt_connector_input(node->connector+c))
        {
          if(dt_cid_unset(node->connector[c].connected))
          {
            snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf), "kernel %"PRItkn"_%"PRItkn"_%"PRItkn":%"PRItkn" is not connected!",
                dt_token_str(node->name), dt_token_str(node->module->inst),
                dt_token_str(node->kernel), dt_token_str(node->connector[c].name));
            graph->gui_msg = graph->gui_msg_buf;
            return VK_INCOMPLETE;
          }
        }
      }
    }

    // detect graphics nodes and choose the right command pool / submission queue for this run.
    // s_node_graphics (draw, overlay) use vkCmdDraw* which requires the graphics queue family;
    // a compute-only queue family does not support these commands.
    graph->use_graphics_queue = 0;
    if(graph->command_pool_gfx) // only relevant when families differ
      for(int i=0;i<cnt;i++)
        if(graph->node[nodeid[i]].type == s_node_graphics)
          { graph->use_graphics_queue = 1; break; }

    // potentially free/re-allocate memory, create buffers, images, image_views, and descriptor sets:
    int dynamic_array = 0;
    QVKR(dt_graph_run_nodes_allocate(graph, &run, nodeid, cnt, &dynamic_array));

    // upload all source data to staging memory
    QVKR(dt_graph_run_nodes_upload(graph, run, nodeid, cnt, module_flags, dynamic_array));

    // now upload uniform data before submitting the command buffer. this runs
    // on module scope, but needs to interlude here, so ray tracing nodes can
    // cut the tri_cnt of dynamic geo that is known after upload. animated nodes
    // should do this in commit_params (and need to find out their respective node
    // from the modules). this calls commit_params:
    QVKR(dt_graph_run_modules_upload_uniforms(graph, run));

    // record command buffer, including memory barriers for transfers (to uniforms and staging)
    QVKR(dt_graph_run_nodes_record_cmd(graph, run, nodeid, cnt, module_flags));
  } // end scope, done with nodes

  if(run & s_graph_run_alloc)
  { // output memory statistics if we did any allocation at all
    for(int i=0;i<32;i++) {
      if((1<<i) & graph->heap_mask) {
        dt_log(s_log_mem, "heap %d: peak rss %g MB vmsize %g MB",
            i,
            graph->heap[i].peak_rss/(1024.0*1024.0),
            graph->heap[i].vmsize  /(1024.0*1024.0));
      }
    }
  }

  double clock_end = dt_time();
  dt_log(s_log_perf, "record cmd buffer:\t%8.3f ms", 1000.0*(clock_end - clock_beg));

  if(run & s_graph_run_record_cmd_buf)
  {
    graph->process_dbuffer[buf_curr] = MAX(graph->process_dbuffer[0], graph->process_dbuffer[1]) + 1;
    // we add one more command list working on the buf_curr double buffer with write access
    VkTimelineSemaphoreSubmitInfo timeline_info = {
      .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .waitSemaphoreValueCount   = 1,
      .pWaitSemaphoreValues      = &graph->display_dbuffer[buf_curr], // we want to write this buffer, wait for last display command to read it
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues    = &graph->process_dbuffer[buf_curr], // lock buf_curr for writing, this signal will remove the lock
    };
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkCommandBuffer cmd_buf = dt_graph_cmd_buf(graph);
    VkSubmitInfo submit = {
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &cmd_buf,
      .pNext                = &timeline_info,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = &graph->semaphore_display,
      .pWaitDstStageMask    = &wait_stage,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = &graph->semaphore_process,
    };

    // submit to the graphics queue when graphics nodes are present (they require it),
    // otherwise use the async compute queue for better UI responsiveness.
    qvk_queue_name_t submit_queue = graph->use_graphics_queue ? s_queue_graphics : graph->queue_name;
    QVKLR(&qvk.queue[qvk.qid[submit_queue]].mutex,
        vkQueueSubmit(qvk.queue[qvk.qid[submit_queue]].queue, 1, &submit, 0));

    VkSemaphoreWaitInfo wait_info = {
      .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores    = &graph->semaphore_process,
      .pValues        = &graph->process_dbuffer[buf_curr],
    };
    if(run & s_graph_run_wait_done) // no timeout
      QVKR(vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX));
  }
  
  // download sink data from GPU to CPU
  dt_graph_run_nodes_download(graph, run, module_flags);

  if(dt_log_global.mask & s_log_perf)
  {
    int q = buf_prev;
    if(run & s_graph_run_wait_done) q = buf_curr; // if we're synchronous, use the one we just waited for
    else
    { // async, have to wait for previous queries to come back:
      VkSemaphoreWaitInfo wait_info = {
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores    = &graph->semaphore_process,
        .pValues        = &graph->process_dbuffer[buf_prev],
      };
      QVKR(vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX));
    }
    if(graph->query[q].cnt) // could store the results just once, but for separation of concerns they are part of the struct:
      QVKR(vkGetQueryPoolResults(qvk.device, graph->query[q].pool,
          0, graph->query[q].cnt,
          sizeof(graph->query[q].pool_results[0]) * graph->query[q].max,
          graph->query[q].pool_results,
          sizeof(graph->query[q].pool_results[0]),
          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

    uint64_t accum_time = 0;
    dt_token_t last_name = 0;
    for(int i=0;i<graph->query[q].cnt;i+=2)
    {
      if(i < graph->query[q].cnt-2 && (
         graph->query[q].name[i] == last_name || !last_name ||
         graph->query[q].name[i] == dt_token("shared") || last_name == dt_token("shared")))
        accum_time += graph->query[q].pool_results[i+1] - graph->query[q].pool_results[i];
      else
      {
        if(i && accum_time > graph->query[q].pool_results[i-1] - graph->query[q].pool_results[i-2])
          dt_log(s_log_perf, "sum %"PRItkn":\t%8.3f ms",
              dt_token_str(last_name),
              accum_time * 1e-6 * qvk.ticks_to_nanoseconds);
        if(i < graph->query[q].cnt-2)
          accum_time = graph->query[q].pool_results[i+1] - graph->query[q].pool_results[i];
      }
      last_name = graph->query[q].name[i];
      // i think this is the most horrible line of printf i've ever written:
      dt_log(s_log_perf, "%-*.*s %-*.*s:\t%8.3f ms",
          8, 8, dt_token_str(graph->query[q].name  [i]),
          8, 8, dt_token_str(graph->query[q].kernel[i]),
          (graph->query[q].pool_results[i+1]-
          graph->query[q].pool_results[i])* 1e-6 * qvk.ticks_to_nanoseconds);
    }
    if(graph->query[q].cnt)
    {
      graph->query[q].last_frame_duration = (graph->query[q].pool_results[graph->query[q].cnt-1]-graph->query[q].pool_results[0])*1e-6 * qvk.ticks_to_nanoseconds;
      dt_log(s_log_perf, "total time:\t%8.3f ms", graph->query[q].last_frame_duration);
    }
  }
  graph->runflags = 0;
  return VK_SUCCESS;
}

dt_node_t *
dt_graph_get_display(
    dt_graph_t *g,
    dt_token_t  which)
{
  // TODO: cache somewhere on graph?
  for(int n=0;n<g->num_nodes;n++)
  {
    if((g->node[n].module->name == dt_token("display") ||
        g->node[n].module->name == dt_token("thumb")) &&
        g->node[n].module->inst == which)
      return g->node+n;
  }
  return 0;
}

dt_connector_image_t*
dt_graph_connector_image(
    dt_graph_t *graph,
    int         nid,    // node id
    int         cid,    // connector id
    int         array,  // array index
    int         dbuf)   // double buffer index
{
  dt_cid_t owner = dt_connector_find_owner(graph, (dt_cid_t){nid, cid});
  if(graph->node[nid].conn_image[cid] == -1 || dt_cid_unset(owner))
  {
    dt_log(s_log_err, "requesting disconnected link %" PRItkn ":%" PRItkn ":%" PRItkn,
        dt_token_str(graph->node[nid].name), dt_token_str(graph->node[nid].kernel), dt_token_str(graph->node[nid].connector[cid].name));
    return 0;
  }
  int frame = dbuf;
  frame %= MAX(1, graph->node[owner.i].connector[owner.c].frames);
  return graph->conn_image_pool +
    graph->node[nid].conn_image[cid] + MAX(1,graph->node[nid].connector[cid].array_length) * frame + array;
}

void
dt_graph_repurpose(dt_graph_t *g)
{
  if(!g->module) return;
#ifdef DEBUG_MARKERS
  dt_stringpool_reset(&g->debug_markers);
#endif
  if(graph_wait_gpu(g, "graph_repurpose") != VK_SUCCESS) return;
  graph_teardown_modules(g);
  // clear logical allocators
  for(int i=0;i<32;i++) dt_vkalloc_nuke(&g->heap[i]);
  graph_destroy_per_image_resources(g);
  // free memory because this can be sizable:
  for(int i=0;i<32;i++)
  {
    vkFreeMemory(qvk.device, g->vkmem[i], 0);
    g->vkmem[i] = 0;
    g->vkmem_size[i] = 0;
  }
  vkFreeMemory(qvk.device, g->vkmem_uniform, 0);
  g->vkmem_uniform = 0;
  g->vkmem_uniform_size = 0;
  // reset command pool (reuse buffers, skip destroy/recreate)
  vkResetCommandPool(qvk.device, g->command_pool, 0);
  if(g->command_pool_gfx)
    vkResetCommandPool(qvk.device, g->command_pool_gfx, 0);
  // reset query pool software counters; hardware reset is done inline via vkCmdResetQueryPool
  for(int i=0;i<2;i++)
    g->query[i].cnt = 0;
  // reset logical state; keep Vulkan handles (semaphores, command pool, query pools, vkmem)
  memset(g->module, 0, sizeof(dt_module_t) * g->num_modules);
  memset(g->node,   0, sizeof(dt_node_t)   * g->num_nodes);
  g->num_modules    = 0;
  g->num_nodes      = 0;
  g->params_end     = 0;
  g->conn_image_end = 0;
  g->frame_cnt      = 1;
  g->lod_scale      = 1;
  g->active_module  = -1;
  g->double_buffer  = 0;  // write-side selector, independent of semaphore timeline
  g->gui_attached   = 0;
  g->thumbnail_image = 0; // owned externally
  g->history_item_cur = 0;
  g->history_item_end = 0;
  // keep display_dbuffer/process_dbuffer: semaphore signal values must be
  // strictly increasing, so the next run must continue from the current values
}

void
dt_graph_apply_keyframes(
    dt_graph_t *g)
{
  for(int m=0;m<g->num_modules;m++)
  {
    if(g->module[m].name == 0) continue; // skip deleted modules
    dt_keyframe_t *kf = g->module[m].keyframe;
    if(g->module[m].keyframe_cnt) for(int p=0;p<g->module[m].so->num_params;p++)
    {
      int ki = -1, kiM = -1; // find ki.f <= f < kiM.f
      for(int i=0;i<g->module[m].keyframe_cnt;i++)
      { // search for max frame smaller than current frame with same param
        if(kf[i].param == g->module[m].so->param[p]->name)
        {
          if(ki == -1) ki = i; // always accept if we don't have any so far
          else if(kf[ki].frame >  g->frame && kf[i].frame < kf[ki].frame) // if we only have an emergency frame so far use the <
            ki = i;
          else if(kf[ki].frame <= g->frame && kf[i].frame <= g->frame && kf[i].frame > kf[ki].frame) // if we have one that is actually lower the new one needs to be <= too
            ki = i;
        }
      }
      if(ki == -1) continue; // no keyframe for this parameter

      for(int i=0;i<g->module[m].keyframe_cnt;i++)
      { // now search for later frame to interpolate
        if(kf[i].param == g->module[m].so->param[p]->name)
          if(kf[i].frame > g->frame && (kiM == -1 || kf[kiM].frame > kf[i].frame)) kiM = i;
      }
      if(kiM == ki) kiM = -1; // don't interpolate same frame
      int parid = dt_module_get_param(g->module[m].so, g->module[m].so->param[p]->name);
      const dt_ui_param_t *p = g->module[m].so->param[parid];
      uint8_t *pdat = g->module[m].param + p->offset;
      uint8_t *fdat = kf[ki].data;
      size_t els = dt_ui_param_size(p->type, 1);
      const float t = dt_anim_warp((g->frame - kf[ki].frame)/(float)(kf[kiM].frame - kf[ki].frame), kf[kiM].anim);
      if(kiM >= 0 && p->type == dt_token("float")) // TODO: don't search for kiM above in the other cases?
      { // interpolate generic floating point parameters
        float *dst = (float *)pdat, *src0 = (float *)fdat, *src1 = (float *)kf[kiM].data;
        for(int i=kf[ki].beg;i<kf[ki].end;i++)
          dst[i] = t * src1[i-kf[ki].beg] + (1.0f-t) * src0[i-kf[ki].beg];
      }
      else if(kiM >= 0 && p->name == dt_token("draw"))
      { // interpolate drawn list of vertices
        uint32_t *dst = (uint32_t *)pdat, *src0 = (uint32_t *)fdat, *src1 = (uint32_t *)kf[kiM].data;
        int vcnt = MIN(src0[0], src1[0]); // can only interpolate what we have on both ends
        dt_draw_vert_t *vd = (dt_draw_vert_t *)(dst +1);
        dt_draw_vert_t *v0 = (dt_draw_vert_t *)(src0+1);
        dt_draw_vert_t *v1 = (dt_draw_vert_t *)(src1+1);
        for(int i=0;i<vcnt;i++)
        {
          if(dt_draw_eq(v0[i], dt_draw_endmarker()) ||
             dt_draw_eq(v1[i], dt_draw_endmarker()))
          { // use symmetric end markers
            vd[i] = dt_draw_endmarker();
          }
          else
          { // interpolate properties
            // XXX FIXME: beg and end are currently not supported. once we have a "draw" type they should mean
            // XXX FIXME: vertex indices, i.e. this here would need to be v0[i - kf[ki].beg] and the loop above
            // XXX FIXME: would need to be MIN(vcnt, kf[ki].end-kf[ki].beg)
            vd[i] = dt_draw_mix(v0[i], v1[i], t);
          }
        }
        dst[0] = (int)(src0[0] * (1.0f-t) + src1[0] * t + 0.5f); // interpolate vertex count, round
        if(src0[0] < src1[0]) for(int i=vcnt;i<dst[0];i++) vd[i] = v1[i];
        if(src1[0] < src0[0]) for(int i=vcnt;i<dst[0];i++) vd[i] = v0[i];
        vd[dst[0]-1] = dt_draw_endmarker();
        g->module[m].flags = s_module_request_read_source; // make sure the draw list is updated
      }
      else
      { // apply directly
        memcpy(pdat, fdat + els*kf[ki].beg, els*(kf[ki].end-kf[ki].beg));
      }
    }
  }
}
