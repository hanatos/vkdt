#include "core/log.h"
#include "gui/gui.h"
#include "pipe/modules/api.h"
#include "pipe/graph.h"
#include "pipe/graph-run-nodes-allocate.h"
#include "gui/graph-display.h"

void
dt_graph_display_image_cleanup(
    dt_graph_display_images_t *dspy,
    dt_graph_display_image_t   *d)
{
  if(d->con.image)      vkDestroyImage      (qvk.device, d->con.image,      0);
  if(d->con.image_view) vkDestroyImageView  (qvk.device, d->con.image_view, 0);
  if(d->mem)            vkFreeMemory        (qvk.device, d->mem,            0);
  if(d->dset)           vkFreeDescriptorSets(qvk.device, dspy->dset_pool, 1, &d->dset);
  for(int m=0;m<LENGTH(d->con.mipmap_views);m++)
    if(d->con.mipmap_views[m]) vkDestroyImageView(qvk.device, d->con.mipmap_views[m], 0);
  memset(d, 0, sizeof(*d));
}

int // return 1 if display image is compatible with given display node
dt_graph_display_image_check_compat(
    dt_graph_display_image_t *d,
    dt_graph_t               *graph,
    dt_node_t                *node)  // the display node
{
  int wd = node->connector[0].roi.wd;
  int ht = node->connector[0].roi.ht;
  // number of mip levels is determined by resolution, not checking explicitly
  return d->con.wd == wd && d->con.ht == ht &&
    d->format == node->connector[0].format &&
    d->chan   == node->connector[0].chan;
}

VkResult
dt_graph_display_image_init(
    dt_graph_display_image_t *d,
    dt_graph_t               *graph,
    dt_node_t                *node)  // the display node
{
  if(!graph->dspy) return VK_INCOMPLETE;
  int wd = node->connector[0].roi.wd;
  int ht = node->connector[0].roi.ht;
  d->con.wd = wd;
  d->con.ht = ht;
  d->format = node->connector[0].format;
  d->chan   = node->connector[0].chan;
  d->con.mip_levels = MIN(4, (uint32_t)(log2f(MAX(wd, ht))) + 1);
  uint32_t queue_indices[] = { qvk.queue_family_graphics, qvk.queue_family_compute };
  VkFormat format = dt_connector_vkformat(node->connector);
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = wd,
      .height = ht,
      .depth  = 1
    },
    .mipLevels             = d->con.mip_levels,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_OPTIMAL,
    .usage                 = 
      VK_IMAGE_USAGE_STORAGE_BIT
      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  if(queue_indices[0] != queue_indices[1])
  {
    images_create_info.sharingMode           = VK_SHARING_MODE_CONCURRENT;
    images_create_info.queueFamilyIndexCount = 2;
    images_create_info.pQueueFamilyIndices   = queue_indices;
  }
  QVKR(vkCreateImage(qvk.device, &images_create_info, 0, &d->con.image));
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(qvk.device, d->con.image, &mem_req);
  dt_check_device_allocation(mem_req.size, 0);
  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = mem_req.size,
    .memoryTypeIndex = qvk_memory_get_device(),
  };
  QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &d->mem));
  QVKR(vkBindImageMemory(qvk.device, d->con.image, d->mem, 0));
  VkImageViewCreateInfo images_view_create_info = {
    .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType   = VK_IMAGE_VIEW_TYPE_2D,
    .format     = format,
    .image      = d->con.image,
    .subresourceRange = {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = d->con.mip_levels,
      .baseArrayLayer = 0,
      .layerCount     = 1
    },
  };
  QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &d->con.image_view));
  for (uint32_t m = 0; m < d->con.mip_levels; m++)
  {
    images_view_create_info.subresourceRange.baseMipLevel = m;
    images_view_create_info.subresourceRange.levelCount = 1;
    if(d->con.mipmap_views[m]) vkDestroyImageView(qvk.device, d->con.mipmap_views[m], 0);
    QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &d->con.mipmap_views[m]));
  }

  VkDescriptorSetAllocateInfo dset_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = graph->dspy->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &node->dset_layout,
  };
  QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &d->dset));
  VkDescriptorImageInfo img_info = {
    .sampler     = qvk.tex_sampler_dspy,
    .imageView   = d->con.image_view,
    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };
  VkWriteDescriptorSet img_dset = {
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet          = d->dset,
    .dstBinding      = 0,
    .descriptorCount = 1,
    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo      = &img_info,
  };
  vkUpdateDescriptorSets(qvk.device, 1, &img_dset, 0, NULL);
  return VK_SUCCESS;
}

void
dt_graph_display_image_cmd_copy(
    dt_graph_display_image_t *d,     // pointing to the display image to write to
    uint64_t                  val,   // timeline value to set on d
    dt_graph_t               *graph, // the graph with the descriptor set pool
    dt_node_t                *node)  // the display node
{
  // display maybe disconnected/broken?
  if(!node->connector[0].roi.wd && !node->connector[0].roi.ht) return;
  VkCommandBuffer cmd_buf = dt_graph_cmd_buf(graph);
  if(!dt_graph_display_image_check_compat(d, graph, node))
  { // cleanup and allocate now
    dt_graph_display_image_cleanup(graph->dspy, d);
    dt_graph_display_image_init(d, graph, node);
    BARRIER_IMG_LAYOUT(d->con.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, d->con.mip_levels);
  }
  d->val = val;

  dt_connector_image_t *con_node = dt_graph_connector_image(graph, node - graph->node, 0, 0, graph->double_buffer);
  VkImageCopy cpy = {
    .srcSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .dstSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .extent = { d->con.wd, d->con.ht, 1 },
  };
  // this can apparently run in compute only, if we only copy colour:
  vkCmdCopyImage(cmd_buf, con_node->image, VK_IMAGE_LAYOUT_GENERAL, d->con.image, VK_IMAGE_LAYOUT_GENERAL, 1, &cpy);
  dt_graph_generate_mipmaps(graph, 0, 0, &d->con);
}

#if 0
int // return 1 if image with this timeline value is ready for display
dt_graph_display_image_ready_for_display(
    dt_graph_t *g,
    uint64_t    val)
{
  uint64_t value;
  VkResult res = vkGetSemaphoreCounterValue(qvk.device, g->semaphore_process, &value);
  if(res != VK_SUCCESS) return 0;
  return val <= value;
}
#endif

// do this all the time, potentially not incrementing image timeline values:
uint64_t 
dt_graph_display_acquire_for_display(
    dt_graph_t *g)
{
  dt_graph_display_images_t *dspy = g->dspy;
  if(!dspy) // not inited yet maybe?
    return -1u; // this will lead to no dset
  // image is done processing (signalled by graph dispatch)
  // query semaphore value of graph dispatch signal. don't ever wait, just use what we have:
  uint64_t value;
  VkResult res = vkGetSemaphoreCounterValue(qvk.device, g->semaphore_process, &value);
  if(res == VK_SUCCESS && value > dspy->timeline_display)
    dspy->timeline_display = value;
  // there is always a buffer with the given value
  return dspy->timeline_display;
}

// only do this if we're actually processing anything new:
uint64_t 
dt_graph_display_acquire_for_processing(
    dt_graph_t *graph)
{
  dt_graph_display_images_t *dspy = graph->dspy;
  uint64_t value_process = 0; // what processing semaphore finished
  VkResult res = vkGetSemaphoreCounterValue(qvk.device, graph->semaphore_process, &value_process);

  if(!dspy)
  { // no display/ui interaction, return good semaphore value, don't double buffer the output
    return value_process + 1;
  }
  if(res != VK_SUCCESS) value_process = MAX(1, dspy->timeline_process) - 1; // well, fuck

  assert(value_process <= dspy->timeline_process);
  assert(dspy->timeline_display <= dspy->timeline_process);

  int slot = -1;
  for(int i=0;i<3;i++)
  { // find display image triple buffer slot for us
    uint64_t tv = dspy->display_image[0][i].val;   // run on main output, the others follow the same logic
    int free = 0;                         // assume still locked
    if(tv <= value_process) free = 1;     // all buffers that finished processing are kinda free
    if(dspy->timeline_display && tv >= dspy->timeline_display) free = 0; // we anticipate to pick that up in the future!
    if(tv && dt_gui_display_in_use(tv)) free = 0; // this is overly conservative / cpu synced
    if(free && slot >= 0) // already have what we need, locked buffer for display + new buffer for processing. the other one can go:
      for(int k=0;k<s_graph_display_cnt;k++)
        dt_graph_display_image_cleanup(dspy, &dspy->display_image[k][i]);
    else if(free && slot < 0) slot = i;
  }

  assert(slot >= 0);

  // this is what we want to stamp on the new image buffer:
  dspy->timeline_process++;

  dt_token_t dn[] = {
    dt_token("main"),
    dt_token("hist"),
    dt_token("dspy"),
    dt_token("view0"),
    dt_token("view1"),
  };

  int cnt = 0;
  dt_node_t *const arr = graph->node;
  const int arr_cnt = graph->num_nodes;
  uint32_t nodeid[2000];
  if(sizeof(nodeid)/sizeof(nodeid[0]) < graph->num_nodes)
  {
    dt_log(s_log_pipe|s_log_err, "too many nodes in graph!");
    return 0;
  }
#define TRAVERSE_POST {\
    nodeid[cnt++] = curr;\
  }
#include "pipe/graph-traverse.inc"
  // TODO cache display nodes after create nodes?
  for(int n=0;n<cnt;n++)
  {
    dt_node_t *node = graph->node + nodeid[n];
    if(node->module->name == dt_token("display"))
    {
      for(int k=0;k<s_graph_display_cnt;k++)
      {
        if(node->module->inst == dn[k])
        { // init/copy buffer:
          dt_graph_display_image_cmd_copy(dspy->display_image[k] + slot, dspy->timeline_process, graph, node);
        }
      }
    }
  }
  return dspy->timeline_process;
}

int
dt_graph_display_have_free_process(
    dt_graph_t *graph)
{
  dt_graph_display_images_t *dspy = graph->dspy;
  if(!dspy) return 0; // this is only called from gui and the rest makes no sense

  uint64_t value_process = 0; // what processing semaphore finished
  VkResult res = vkGetSemaphoreCounterValue(qvk.device, graph->semaphore_process, &value_process);
  if(res != VK_SUCCESS) return 0;

  // also we depend on free display image slots. if the ui is spamming dispatches
  // that lock a lot of buffers, we'll need to wait for some of them to finish / pick up the other buffer
  int slot = -1;
  for(int i=0;i<3;i++)
  { // find display image triple buffer slot for us
    uint64_t tv = dspy->display_image[0][i].val;   // run on main output, the others follow the same logic
    int free = 0;                         // assume still locked
    if(tv <= value_process) free = 1;     // all buffers that finished processing are kinda free
    if(dspy->timeline_display && tv >= dspy->timeline_display) free = 0; // we anticipate to pick that up in the future!
    if(tv && dt_gui_display_in_use(tv))
      free = 0; // this is overly conservative / cpu synced
    if(free && slot < 0) slot = i;
  }
  if(slot < 0) return 0;

  // we have two gpu processes, synced by barriers (if nothing else).
  // if value of the semaphore is our current timeline_process, all of these are finished.
  // timeline_process might be ahead of the semaphore value, that's the number of still running
  // dispatches in flight:
  int64_t in_flight = dspy->timeline_process - value_process;
  return in_flight < 2;
}

VkDescriptorSet
dt_graph_display_get_dset(
    dt_graph_t *g,
    dt_token_t  name)
{
  if(!g->dspy) return (VkDescriptorSet){0};
  dt_token_t dn[] = {
    dt_token("main"),
    dt_token("hist"),
    dt_token("dspy"),
    dt_token("view0"),
    dt_token("view1"),
  };
  uint64_t tv = 0; // timeline value / frame id. we want to pick the most recent
  VkDescriptorSet ret = (VkDescriptorSet){0};
  for(int i=0;i<s_graph_display_cnt;i++) if(name == dn[i]) for(int k=0;k<3;k++)
  {
    if((g->dspy->display_image[i][k].val > tv) &&
       (g->dspy->display_image[i][k].val == g->dspy->timeline_display))
    {
      tv  = g->dspy->display_image[i][k].val;
      ret = g->dspy->display_image[i][k].dset;
    }
  }
  return ret;
}

VkResult dt_graph_display_images_init(dt_graph_display_images_t *dspy)
{
  VkDescriptorPoolSize pool_size = {
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 3*s_graph_display_cnt,
  };
  VkDescriptorPoolCreateInfo pool_info = {
    .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = 1,
    .pPoolSizes    = &pool_size,
    .maxSets       = 3*s_graph_display_cnt,
  };
  QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &dspy->dset_pool));
  return VK_SUCCESS;
}

void dt_graph_display_images_cleanup(dt_graph_display_images_t *dspy)
{
  for(int k=0;k<s_graph_display_cnt;k++)
    for(int i=0;i<3;i++)
      dt_graph_display_image_cleanup(dspy, &dspy->display_image[k][i]);
  vkDestroyDescriptorPool(qvk.device, dspy->dset_pool, 0);
}
