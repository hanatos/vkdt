#include "core/log.h"
#include "pipe/thumbnails.h"
#include "qvk/qvk.h"
#include "pipe/graph-io.h"
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>


void
dt_thumbnails_init(
    dt_thumbnails_t *tn)
{
  memset(tn, 0, sizeof(*tn));

  dt_graph_init(&tn->graph);

  tn->thumb_wd = 400;
  tn->thumb_ht = 300;
}

void
dt_thumbnails_cleanup(
    dt_thumbnails_t *tn)
{
  // TODO: delete it all again
}

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

VkResult
dt_thumbnails_create(
    dt_thumbnails_t *tn,
    const char      *dirname)
{
  // TODO: make sure we freed all resources
  tn->thumb_max = 0;
  // walk twice to count images and alloc resources accordingly.
  // first pass: count images.
  DIR *dp = opendir(dirname);
  if(!dp)
  {
    dt_log(s_log_err, "could not open directory '%s'!", dirname);
    return VK_INCOMPLETE;
  }
  struct dirent *ep;
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG) continue; // accept DT_LNK, too?
    if(!accept_filename(ep->d_name)) continue;
    tn->thumb_max++;
  }
  if(tn->thumb_max == 0)
  {
    dt_log(s_log_err, "no usable images in directory '%s'!", dirname);
    return VK_INCOMPLETE;
  }
  tn->thumb_cnt = 0;
  tn->thumb = malloc(sizeof(dt_thumbnail_t)*tn->thumb_max);
  memset(tn->thumb, 0, sizeof(dt_thumbnail_t)*tn->thumb_max);
  // need at least one extra slot to catch free block (if contiguous, else more)
  dt_vkalloc_init(&tn->alloc, tn->thumb_max + 10);

  // XXX what are good "combinations of parameters" that the validation layer will allow??
  // VkFormat format = VK_FORMAT_R8G8B8A8_UINT;//VK_FORMAT_R16G16B16A16_SFLOAT;// VK_FORMAT_R32_SFLOAT; // XXX VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;// VK_FORMAT_R32_SFLOAT; // XXX VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  // VkFormat format = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = tn->thumb_wd,
      .height = tn->thumb_ht,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_OPTIMAL,
    .usage                 =
        VK_IMAGE_USAGE_STORAGE_BIT
      // | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
      // | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  uint32_t memory_type_bits = ~0u;
  for(int i=0;i<tn->thumb_max;i++)
  {
    QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &tn->thumb[i].image));
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(qvk.device, tn->thumb[i].image, &mem_req);
    if(memory_type_bits != ~0u && mem_req.memoryTypeBits != memory_type_bits)
      dt_log(s_log_qvk|s_log_err, "memory type bits don't match!");
    memory_type_bits = mem_req.memoryTypeBits;

    dt_vkmem_t *mem = dt_vkalloc(&tn->alloc, mem_req.size, mem_req.alignment);
    assert(mem);
    tn->thumb[i].mem    = mem;
    tn->thumb[i].offset = mem->offset;
  }

  size_t alloc_size = tn->alloc.vmsize;
  dt_log(s_log_gui, "allocating %3.1f MB for thumbnails", alloc_size/(1024.0*1024.0));

  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = alloc_size,
    .memoryTypeIndex = qvk_get_memory_type(memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
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
    .components = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
    },
  };
  VkDescriptorSetAllocateInfo dset_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = tn->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &tn->dset_layout,
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
  for(int i=0;i<tn->thumb_max;i++)
  {
    vkBindImageMemory(qvk.device, tn->thumb[i].image, tn->vkmem, tn->thumb[i].offset);
    images_view_create_info.image = tn->thumb[i].image;

    QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &tn->thumb[i].image_view));
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &tn->thumb[i].dset));

    img_dset.dstSet = tn->thumb[i].dset;
    img_info.imageView = tn->thumb[i].image_view;
    vkUpdateDescriptorSets(qvk.device, 1, &img_dset, 0, NULL);
  }

  // start second pass over file names:
  rewinddir(dp);

  char imgfilename[4096] = {0};
  char cfgfilename[4096] = {0};
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG) continue; // accept DT_LNK, too?
    if(!accept_filename(ep->d_name)) continue;

    snprintf(imgfilename, sizeof(imgfilename), "%s/%s", dirname, ep->d_name);
    fprintf(stderr, "XXX loading thumb for img %s\n", ep->d_name);

    // load individual history stack if any
    // TODO: try this or load default.cfg instead:
    // snprintf(cfgfilename, sizeof(cfgfilename), "%s/%s.cfg", dirname, ep->d_name);
    snprintf(cfgfilename, sizeof(cfgfilename), "default.cfg");
    // TODO: remove all extra displays
    dt_graph_init(&tn->graph);
    int err = dt_graph_read_config_ascii(&tn->graph, cfgfilename);
    if(err)
    {
      dt_log(s_log_err, "could not load graph configuration from '%s'!", cfgfilename);
      continue;
    }

    // set param for rawinput
    // TODO: jpg input?
    // get module
    int modid = dt_module_get(&tn->graph, dt_token("rawinput"), dt_token("01"));
    dt_module_t *mod = tn->graph.module + modid;
    char *param_filename = 0;
    int param_len = 0;
    for(int p=0;p<mod->so->num_params;p++)
    {
      if(mod->so->param[p]->name == dt_token("filename"))
      {
        param_filename = (char *)(mod->param + mod->so->param[p]->offset);
        param_len = mod->so->param[p]->cnt;
        break;
      }
    }
    if(!param_filename)
    {
      dt_log(s_log_err, "config '%s' has no rawinput module!", cfgfilename);
      continue;
    }

    snprintf(param_filename, param_len, "%s", imgfilename);

    // let graph render into our thumbnail:
    tn->graph.thumbnail_image = tn->thumb[tn->thumb_cnt].image;
    tn->graph.thumbnail_wd = tn->thumb_wd;
    tn->graph.thumbnail_ht = tn->thumb_ht;

    // TODO: ask for reduced resolution in the graph:

    if(dt_graph_run(&tn->graph, s_graph_run_all) != VK_SUCCESS)
    {
      dt_log(s_log_err, "running the thumbnail graph failed on image '%s'!", imgfilename);
      continue;
    }

    // TODO: is this init/cleanup cycle needed?
    // it seems the graph should have some stuff in place to reuse/rebuild
    // required vk structs:
    dt_graph_cleanup(&tn->graph);
    tn->thumb_cnt++;
  }

  fprintf(stderr, "XXX done.\n");

  // have multiple dt_graph_t?
  // run without sync/wait and interleave rawspeed and gpu?
  // TODO: test perf
  // TODO: write as jpg export
  // TODO: test perf

  closedir(dp);
  return VK_SUCCESS;
}
