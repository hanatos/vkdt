#include "core/log.h"
#include "db/thumbnails.h"
#include "qvk/qvk.h"
#include "pipe/graph-io.h"
#include "pipe/modules/api.h"
#include "pipe/dlist.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <time.h>


VkResult
dt_thumbnails_init(
    dt_thumbnails_t *tn,
    const int wd,
    const int ht,
    const int cnt,
    const size_t heap_size)
{
  memset(tn, 0, sizeof(*tn));

  tn->thumb_wd = wd,
  tn->thumb_ht = ht,
  tn->thumb_max = cnt;

  tn->thumb_cnt = 0;
  tn->thumb = malloc(sizeof(dt_thumbnail_t)*tn->thumb_max);
  memset(tn->thumb, 0, sizeof(dt_thumbnail_t)*tn->thumb_max);
  // need at least one extra slot to catch free block (if contiguous, else more)
  dt_vkalloc_init(&tn->alloc, tn->thumb_max + 10, heap_size);

  // init lru list
  tn->lru = tn->thumb;
  tn->mru = tn->thumb + tn->thumb_max-1;
  tn->thumb[0].next = tn->thumb+1;
  for(int k=1;k<tn->thumb_max-1;k++)
  {
    tn->thumb[k].next = tn->thumb+k+1;
    tn->thumb[k].prev = tn->thumb+k-1;
  }

  dt_log(s_log_db, "allocating %3.1f MB for thumbnails", heap_size/(1024.0*1024.0));

  // alloc dummy image to get memory type bits and something to display
  VkFormat format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = 24,
      .height = 24,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_LINEAR,
    .usage                 =
        VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage img;
  QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &img));
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(qvk.device, img, &mem_req);
  tn->memory_type_bits = mem_req.memoryTypeBits;
  vkDestroyImage(qvk.device, img, VK_NULL_HANDLE);

  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = heap_size,
    .memoryTypeIndex = qvk_get_memory_type(tn->memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &tn->vkmem));

  // create descriptor pool (keep at least one for each type)
  VkDescriptorPoolSize pool_sizes[] = {{
    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = tn->thumb_max,
  }};

  VkDescriptorPoolCreateInfo pool_info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = LENGTH(pool_sizes),
    .pPoolSizes    = pool_sizes,
    .maxSets       = tn->thumb_max,
  };
  QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &tn->dset_pool));

  // create a descriptor set layout
  VkDescriptorSetLayoutBinding binding = {
    .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount    = 1,
    .stageFlags         = VK_SHADER_STAGE_ALL,
    .pImmutableSamplers = 0,
  };
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings    = &binding,
  };
  QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &tn->dset_layout));

  VkDescriptorSetAllocateInfo dset_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = tn->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &tn->dset_layout,
  };
  for(int i=0;i<tn->thumb_max;i++)
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &tn->thumb[i].dset));
  return VK_SUCCESS;
}

void
dt_thumbnails_cleanup(
    dt_thumbnails_t *tn)
{
  for(int i=0;i<tn->thumb_max;i++)
  {
    if(tn->thumb[i].image)      vkDestroyImage    (qvk.device, tn->thumb[i].image,      0);
    if(tn->thumb[i].image_view) vkDestroyImageView(qvk.device, tn->thumb[i].image_view, 0);
  }
  free(tn->thumb);
  tn->thumb = 0;
  vkDestroyDescriptorSetLayout(qvk.device, tn->dset_layout, 0);
  vkDestroyDescriptorPool     (qvk.device, tn->dset_pool,   0);
  vkFreeMemory                (qvk.device, tn->vkmem,       0);
  dt_vkalloc_cleanup(&tn->alloc);
}

// TODO: move outside, db.c needs it, too
static int
accept_filename(
    const char *f)
{
  // TODO: magic number checks instead.
  const char *f2 = f + strlen(f);
  while(f2 > f && *f2 != '.') f2--;
  return !strcasecmp(f2, ".cr2") ||
         !strcasecmp(f2, ".nef") ||
         !strcasecmp(f2, ".orf") ||
         !strcasecmp(f2, ".arw") ||
         !strcasecmp(f2, ".raf");
}

// process one image and write a .bc1 thumbnail
// return 0 on success
static VkResult
dt_thumbnails_cache_one(
    dt_thumbnails_t *tn,
    const char      *filename) 
{
  if(!accept_filename(filename)) return 1;

  char cfgfilename[1024];
  // load individual history stack if any
  // TODO: try this or load default.cfg instead:
  // snprintf(cfgfilename, sizeof(cfgfilename), "%s/%s.cfg", dirname, ep->d_name);
  snprintf(cfgfilename, sizeof(cfgfilename), "default.cfg");
  dt_graph_init(&tn->graph);
  if(dt_graph_read_config_ascii(&tn->graph, cfgfilename))
  {
    dt_log(s_log_err, "[thm] could not load graph configuration from '%s'!", cfgfilename);
    return 2;
  }

  // set param for rawinput
  // get module
  int modid = dt_module_get(&tn->graph, dt_token("rawinput"), dt_token("01"));
  if(modid < 0 ||
     dt_module_set_param_string(tn->graph.module + modid, dt_token("filename"), filename))
  {
    dt_log(s_log_err, "[thm] config '%s' has no rawinput module!", cfgfilename);
    return 3;
  }

  modid = dt_module_get(&tn->graph, dt_token("bc1out"), dt_token("main"));
  if(modid < 0 ||
     dt_module_set_param_string(tn->graph.module + modid, dt_token("filename"), filename))
  {
    dt_log(s_log_err, "[thm] config '%s' has no bc1out module!", cfgfilename);
    return 3;
  }

  // ask for reduced resolution in the graph:
  tn->graph.output_wd = tn->thumb_wd;
  tn->graph.output_ht = tn->thumb_ht;

  clock_t beg = clock();
  if(dt_graph_run(&tn->graph, s_graph_run_all) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] running the thumbnail graph failed on image '%s'!", filename);
    return 4;
  }
  clock_t end = clock();
  dt_log(s_log_pipe|s_log_perf, "[thm] ran graph in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  dt_graph_cleanup(&tn->graph);
  return VK_SUCCESS;
}

VkResult
dt_thumbnails_cache_directory(
    dt_thumbnails_t *tn,
    const char      *dirname) 
{
  DIR *dp = opendir(dirname);
  if(!dp)
  {
    dt_log(s_log_err, "[thm] could not open directory '%s'!", dirname);
    return VK_INCOMPLETE;
  }
  struct dirent *ep;
  char filename[2048];
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG) continue; // accept DT_LNK, too?
    if(!accept_filename(ep->d_name)) continue;
    snprintf(filename, sizeof(filename), "%s/%s", dirname, ep->d_name);
    (void) dt_thumbnails_cache_one(tn, filename);
  }
  return VK_SUCCESS;
}

// load one previously cached thumbnail to VkImage onto the GPU.
// returns VK_SUCCESS on success
VkResult
dt_thumbnails_load_one(
    dt_thumbnails_t *tn,
    const char      *filename,
    uint32_t        *thumb_index)
{
  char cfgfilename[1024] = {0};
  char imgfilename[1024] = {0};
  snprintf(cfgfilename, sizeof(cfgfilename), "thumb.cfg");
  snprintf(imgfilename, sizeof(imgfilename), "%s.bc1", filename);
  struct stat statbuf = {0};
  if(stat(imgfilename, &statbuf)) return VK_INCOMPLETE;
  if(stat(cfgfilename, &statbuf)) return VK_INCOMPLETE;

  dt_graph_init(&tn->graph);
  if(dt_graph_read_config_ascii(&tn->graph, cfgfilename))
  {
    dt_log(s_log_err, "[thm] could not load graph configuration from '%s'!", cfgfilename);
    dt_graph_cleanup(&tn->graph);
    return VK_INCOMPLETE;
  }

  // TODO: assume mutex is locked from the outside?
  // allocate thumbnail from lru list
  dt_thumbnail_t *th = tn->lru;
  tn->lru = tn->lru->next;             // move head
  DLIST_RM_ELEMENT(th);                // disconnect old head
  tn->mru = DLIST_APPEND(tn->mru, th); // append to end and move tail
  *thumb_index = th - tn->thumb;
  
#if 1 // cache eviction
  // clean up memory in case there was something here:
  if(th->image)      vkDestroyImage(qvk.device, th->image, VK_NULL_HANDLE);
  if(th->image_view) vkDestroyImageView(qvk.device, th->image_view, VK_NULL_HANDLE);
  th->image      = 0;
  th->image_view = 0;
  th->imgid      = -1u;
  th->offset     = -1u;
  if(th->mem)    dt_vkfree(&tn->alloc, th->mem);
  th->mem        = 0;
  // keep dset and prev/next dlist pointers! (i.e. don't memset th)
#endif

  // set param for rawinput
  // get module
  int modid = dt_module_get(&tn->graph, dt_token("bc1input"), dt_token("01"));
  if(modid < 0 ||
     dt_module_set_param_string(tn->graph.module + modid, dt_token("filename"), imgfilename))
  {
    dt_log(s_log_err, "[thm] config '%s' has no bc1in module!", cfgfilename);
    dt_graph_cleanup(&tn->graph);
    return VK_INCOMPLETE;
  }

  // run graph only up to roi computations to get size
  // run all <= create nodes
  dt_graph_run_t run = ~-(s_graph_run_create_nodes<<1);
  if(dt_graph_run(&tn->graph, run) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] failed to run first half of graph!");
    dt_graph_cleanup(&tn->graph);
    return VK_INCOMPLETE;
  }

  // now grab roi size from graph's main output node
  modid = dt_module_get(&tn->graph, dt_token("thumb"), dt_token("main"));
  const int wd = tn->graph.module[modid].connector[0].roi.full_wd;
  const int ht = tn->graph.module[modid].connector[0].roi.full_ht;

  VkFormat format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = wd,
      .height = ht,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_LINEAR,
    .usage                 =
        VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &th->image));
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(qvk.device, th->image, &mem_req);
  if(mem_req.memoryTypeBits != tn->memory_type_bits)
    dt_log(s_log_qvk|s_log_err, "[thm] memory type bits don't match!");

  dt_vkmem_t *mem = dt_vkalloc(&tn->alloc, mem_req.size, mem_req.alignment);
  // TODO: if (!mem) we have not enough memory! need to handle this now (more cache eviction?)
  // TODO: could do batch cleanup in case we need memory:
  // walk lru list from front and kill all contents (see above)
  // but leave list as it is

  assert(mem);
  th->mem    = mem;
  th->offset = mem->offset;

  VkImageViewCreateInfo images_view_create_info = {
    .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType   = VK_IMAGE_VIEW_TYPE_2D,
    .format     = format,
    .subresourceRange = {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1
    },
  };
  VkDescriptorImageInfo img_info = {
    .sampler       = qvk.tex_sampler, // qvk.tex_sampler_nearest,
    .imageLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet img_dset = {
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding      = 0,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo      = &img_info,
  };

  // bind image memory, create image view and descriptor set (used to display later on):
  vkBindImageMemory(qvk.device, th->image, tn->vkmem, th->offset);
  images_view_create_info.image = th->image;
  QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &th->image_view));

  img_dset.dstSet    = th->dset;
  img_info.imageView = th->image_view;
  vkUpdateDescriptorSets(qvk.device, 1, &img_dset, 0, NULL);

  // now run the rest of the graph and copy over VkImage
  // let graph render into our thumbnail:
  tn->graph.thumbnail_image = tn->thumb[tn->thumb_cnt].image;
  // these should already match, let's not mess with rounding errors:
  // tn->graph.output_wd = wd;
  // tn->graph.output_ht = ht;

  clock_t beg = clock();
  // run all the rest we didn't run above
  if(dt_graph_run(&tn->graph, ~run) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] running the thumbnail graph failed on image '%s'!", imgfilename);
    dt_graph_cleanup(&tn->graph);
    return VK_INCOMPLETE;
  }
  clock_t end = clock();
  dt_log(s_log_pipe, "[thm] ran graph in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  dt_graph_cleanup(&tn->graph);
  return VK_SUCCESS;
}
