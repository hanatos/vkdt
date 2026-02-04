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
  dt_vkalloc_init(&g->heap, 16000, ((uint64_t)1)<<40); // bytesize doesn't matter
  dt_vkalloc_init(&g->heap_ssbo, 8000, ((uint64_t)1)<<40);
  dt_vkalloc_init(&g->heap_staging, 100, ((uint64_t)1)<<40);
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
}

void
dt_graph_cleanup(dt_graph_t *g)
{
  if(!g->module) return; // already cleaned up
#ifdef DEBUG_MARKERS
  dt_stringpool_cleanup(&g->debug_markers);
#endif
  g->active_module = -1;
  // need to wait on pending commands for graph processing and potential gui display
  // in case no gui is attached, the display indices should stay at 0 so the wait is a no-op
  const uint64_t wait_value[] = {
    MAX(g->display_dbuffer[0], g->display_dbuffer[1]),
    MAX(g->process_dbuffer[0], g->process_dbuffer[1]),
  };
  VkSemaphore sem[] = {
    g->semaphore_display,
    g->semaphore_process,
  };
  VkSemaphoreWaitInfo wait_info = {
    .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 2,
    .pSemaphores    = sem,
    .pValues        = wait_value,
  };
  VkResult res = vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX);
  // this can only return all good or DEVICE LOST which would be really bad
  if(res != VK_SUCCESS)
  {
    dt_log(s_log_pipe|s_log_err, "[graph_cleanup] lost device while waiting for semaphores!");
    return;
  }

  for(int i=0;i<g->num_modules;i++)
    if(g->module[i].name && g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
  for(int i=0;i<g->num_modules;i++)
  {
    free(g->module[i].keyframe);
    g->module[i].keyframe_size = 0;
    g->module[i].keyframe_cnt = 0;
    g->module[i].keyframe = 0;
  }
  dt_vkalloc_cleanup(&g->heap);
  dt_vkalloc_cleanup(&g->heap_ssbo);
  dt_vkalloc_cleanup(&g->heap_staging);
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
          if(img0->image == img1->image)
          { // it's enough to clean up one replicant, the rest will shut down cleanly later:
            // if(img0->image) fprintf(stderr, "discarding img %lx -- %lx\n", img0->image, img1 ? img1->image : 0);
            *img0 = (dt_connector_image_t){0};
          }
        }
      }
    }
  }
  for(int i=0;i<g->conn_image_end;i++)
  {
    if(g->conn_image_pool[i].buffer)     vkDestroyBuffer(qvk.device,    g->conn_image_pool[i].buffer, VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image)      vkDestroyImage(qvk.device,     g->conn_image_pool[i].image,  VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, g->conn_image_pool[i].image_view, VK_NULL_HANDLE);
    g->conn_image_pool[i].buffer = 0;
    g->conn_image_pool[i].image = 0;
    g->conn_image_pool[i].image_view = 0;
  }
  for(int i=0;i<g->num_nodes;i++)
  {
    for(int j=0;j<g->node[i].num_connectors;j++)
    {
      dt_connector_t *c = g->node[i].connector+j;
      if(c->staging[0]) vkDestroyBuffer(qvk.device, c->staging[0], VK_NULL_HANDLE);
      if(c->staging[1]) vkDestroyBuffer(qvk.device, c->staging[1], VK_NULL_HANDLE);
      c->staging[0] = c->staging[1] = 0;
      if(c->array_alloc)
      { // free any potential residuals of dynamic allocation
        dt_vkalloc_cleanup(c->array_alloc);
        free(c->array_alloc);
        c->array_alloc = 0;
        c->array_alloc_size = 0;
        c->array_mem = 0;
      }
    }
    vkDestroyPipelineLayout     (qvk.device, g->node[i].pipeline_layout,  0);
    vkDestroyPipeline           (qvk.device, g->node[i].pipeline,         0);
    vkDestroyDescriptorSetLayout(qvk.device, g->node[i].dset_layout,      0);
    vkDestroyFramebuffer        (qvk.device, g->node[i].draw_framebuffer, 0);
    vkDestroyRenderPass         (qvk.device, g->node[i].draw_render_pass, 0);
    g->node[i].pipeline_layout = 0;
    g->node[i].pipeline = 0;
    g->node[i].dset_layout = 0;
    g->node[i].draw_framebuffer = 0;
    g->node[i].draw_render_pass = 0;
    dt_raytrace_node_cleanup(g->node + i);
  }
  dt_raytrace_graph_cleanup(g);
  vkDestroyDescriptorPool(qvk.device, g->dset_pool, 0);
  vkDestroyDescriptorSetLayout(qvk.device, g->uniform_dset_layout, 0);
  vkDestroyBuffer(qvk.device, g->uniform_buffer, 0);
  g->dset_pool = 0;
  g->uniform_dset_layout = 0;
  g->uniform_buffer = 0;
  vkFreeMemory(qvk.device, g->vkmem, 0);
  vkFreeMemory(qvk.device, g->vkmem_ssbo, 0);
  vkFreeMemory(qvk.device, g->vkmem_staging, 0);
  vkFreeMemory(qvk.device, g->vkmem_uniform, 0);
  g->vkmem = g->vkmem_ssbo = g->vkmem_staging = g->vkmem_uniform = 0;
  g->vkmem_size = g->vkmem_ssbo_size = g->vkmem_staging_size = g->vkmem_uniform_size = 0;
  vkDestroySemaphore(qvk.device, g->semaphore_display, 0);
  vkDestroySemaphore(qvk.device, g->semaphore_process, 0);
  g->semaphore_display = 0;
  g->semaphore_process = 0;
  if(g->command_pool != VK_NULL_HANDLE)
    vkFreeCommandBuffers(qvk.device, g->command_pool, 2, g->command_buffer);
  g->command_buffer[0] = g->command_buffer[1] = VK_NULL_HANDLE;
  vkDestroyCommandPool(qvk.device, g->command_pool, 0);
  g->command_pool = 0;
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
}

static inline void *
read_file(const char *filename, size_t *len)
{
  FILE *f = dt_graph_open_resource(0, 0, filename, "rb");
  if(!f)
  {
    dt_log(s_log_qvk|s_log_err, "failed to read shader '%s': %s!",
        filename, strerror(errno));
    return 0;
  }
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

// convenience function for debugging in gdb:
void dt_token_print(dt_token_t t)
{
  fprintf(stderr, "token: %"PRItkn"\n", dt_token_str(t));
}

VkResult dt_graph_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run)
{
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

    // potentially free/re-allocate memory, create buffers, images, image_views, and descriptor sets:
    int dynamic_array = 0;
    QVKR(dt_graph_run_nodes_allocate(graph, &run, nodeid, cnt, &dynamic_array));

    // upload all source data to staging memory
    QVKR(dt_graph_run_nodes_upload(graph, run, nodeid, cnt, module_flags, dynamic_array));

    // now upload uniform data before submitting the command buffer. this runs
    // on module scope, but needs to interlude here, so ray tracing nodes can
    // cut the tri_cnt of dynamic geo that is known after upload. animated nodes
    // should do this in commit_params (and need to find out their respective node
    // from the modules)
    QVKR(dt_graph_run_modules_upload_uniforms(graph, run));

    // record command buffer, including memory barriers for transfers (to uniforms and staging)
    QVKR(dt_graph_run_nodes_record_cmd(graph, run, nodeid, cnt, module_flags));
  } // end scope, done with nodes

  if(run & s_graph_run_alloc)
  { // output memory statistics if we did any allocation at all
    dt_log(s_log_mem, "images : peak rss %g MB vmsize %g MB",
        graph->heap.peak_rss/(1024.0*1024.0),
        graph->heap.vmsize  /(1024.0*1024.0));
    dt_log(s_log_mem, "buffers: peak rss %g MB vmsize %g MB",
        graph->heap_ssbo.peak_rss/(1024.0*1024.0),
        graph->heap_ssbo.vmsize  /(1024.0*1024.0));
    dt_log(s_log_mem, "staging: peak rss %g MB vmsize %g MB",
        graph->heap_staging.peak_rss/(1024.0*1024.0),
        graph->heap_staging.vmsize  /(1024.0*1024.0));
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
    VkSubmitInfo submit = {
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &graph->command_buffer[buf_curr],
      .pNext                = &timeline_info,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = &graph->semaphore_display,
      .pWaitDstStageMask    = &wait_stage,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = &graph->semaphore_process,
    };

    QVKLR(&qvk.queue[qvk.qid[graph->queue_name]].mutex,
        vkQueueSubmit(qvk.queue[qvk.qid[graph->queue_name]].queue, 1, &submit, 0));

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

void dt_graph_reset(dt_graph_t *g)
{
  qvk_queue_name_t qname = g->queue_name;
  dt_graph_cleanup(g);
  dt_graph_init(g, qname);

#if 0 // just too hard to maintain:
#ifdef DEBUG_MARKERS
  dt_stringpool_reset(&g->debug_markers);
#endif
  dt_raytrace_graph_reset(g);
  g->gui_attached = 0;
  g->gui_msg = 0;
  g->active_module = -1;
  g->lod_scale = 0;
  g->runflags = 0;
  g->frame = 0;
  g->thumbnail_image = 0;
  g->query[0].cnt = g->query[1].cnt = 0;
  g->params_end = 0;
  g->double_buffer = 0;
  for(int i=0;i<g->num_modules;i++)
    if(g->module[i].name && g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
  for(int i=0;i<g->conn_image_end;i++)
  {
    if(g->conn_image_pool[i].image)      vkDestroyImage(qvk.device,     g->conn_image_pool[i].image, VK_NULL_HANDLE);
    if(g->conn_image_pool[i].buffer)     vkDestroyBuffer(qvk.device,    g->conn_image_pool[i].buffer, VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, g->conn_image_pool[i].image_view, VK_NULL_HANDLE);
    g->conn_image_pool[i].image = 0;
    g->conn_image_pool[i].buffer = 0;
    g->conn_image_pool[i].image_view = 0;
  }
  for(int i=0;i<g->num_nodes;i++)
  {
    for(int j=0;j<g->node[i].num_connectors;j++)
    {
      dt_connector_t *c = g->node[i].connector+j;
      if(c->staging) vkDestroyBuffer(qvk.device, c->staging, VK_NULL_HANDLE);
      c->staging = 0;
      if(c->array_alloc)
      { // free any potential residuals of dynamic allocation
        dt_vkalloc_cleanup(c->array_alloc);
        free(c->array_alloc);
        c->array_alloc = 0;
        c->array_alloc_size = 0;
        c->array_mem = 0;
      }
      *c = (dt_connector_t){0};
    }
    vkDestroyPipelineLayout     (qvk.device, g->node[i].pipeline_layout,  0);
    vkDestroyPipeline           (qvk.device, g->node[i].pipeline,         0);
    vkDestroyDescriptorSetLayout(qvk.device, g->node[i].dset_layout,      0);
    vkDestroyFramebuffer        (qvk.device, g->node[i].draw_framebuffer, 0);
    vkDestroyRenderPass         (qvk.device, g->node[i].draw_render_pass, 0);
    g->node[i].pipeline_layout = 0;
    g->node[i].pipeline = 0;
    g->node[i].dset_layout = 0;
    g->node[i].draw_framebuffer = 0;
    g->node[i].draw_render_pass = 0;
    dt_raytrace_node_cleanup(g->node + i);
  }
  g->conn_image_end = 0;
  g->num_nodes = 0;
  g->num_modules = 0;
#endif
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
