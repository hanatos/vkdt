#pragma once

#include "pipe/graph.h"
#include "pipe/alloc.h"
#include <vulkan/vulkan.h>

// render out a lot of images from the same
// graph object that is re-initialised to accomodate
// the needs of the images. the results are kept in a
// separate memory block and can be displayed in the gui.
// 
// this stores a list of thumbnails, the correspondence
// to imgid needs to be done externally (image struct with pointer to here)
//
// TODO: just map to a list of loaded images? load from dummy graph via jpg on disk vs full raw?
// maybe:
// 1) create thumbnails and default history here
// ~/.config/vkdt/<full path from root>/imgname.history
// ~/.cache/vkdt/<full path from root>/imgname_hash.jpg
// 2) create thumbnails in memory from jpginput->display

typedef struct dt_thumbnail_t
{
  VkDescriptorSet       dset;
  VkImage               image;
  VkImageView           image_view;
  dt_vkmem_t           *mem;
  uint64_t              offset;
}
dt_thumbnail_t;

typedef struct dt_thumbnails_t
{
  dt_graph_t graph;

  int thumb_wd, thumb_ht;

  dt_vkalloc_t          alloc;
  VkDeviceMemory        vkmem;
  VkDescriptorPool      dset_pool;
  VkDescriptorSetLayout dset_layout;
  dt_thumbnail_t       *thumb;
  int                   thumb_cnt;
  int                   thumb_max;
}
dt_thumbnails_t;

void dt_thumbnails_init   (dt_thumbnails_t *tn);
void dt_thumbnails_cleanup(dt_thumbnails_t *tn);

VkResult dt_thumbnails_create(dt_thumbnails_t *tn, const char *dirname);
VkResult dt_thumbnails_cache_directory(dt_thumbnails_t *tn, const char      *dirname);
