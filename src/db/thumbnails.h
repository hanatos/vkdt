#pragma once

#include "pipe/graph.h"
#include "pipe/alloc.h"
#include "core/threads.h"

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
// ~/.cache/vkdt/<full path from root>/imgname_hash.bc1

typedef struct dt_thumbnail_t
{
  VkDescriptorSet        dset;
  VkImage                image;
  VkImageView            image_view;
  dt_vkmem_t            *mem;
  uint64_t               offset;
  struct dt_thumbnail_t *prev;    // dlist for lru cache
  struct dt_thumbnail_t *next;
  uint32_t               imgid;   // index into images->image[] or -1u
}
dt_thumbnail_t;

#define DT_THUMBNAILS_THREADS 2
typedef struct dt_thumbnails_t
{
  dt_graph_t            graph[DT_THUMBNAILS_THREADS];

  int                   thumb_wd;
  int                   thumb_ht;

  dt_vkalloc_t          alloc;
  uint32_t              memory_type_bits;
  VkDeviceMemory        vkmem;
  VkDescriptorPool      dset_pool;
  VkDescriptorSetLayout dset_layout;
  dt_thumbnail_t       *thumb;
  int                   thumb_cnt;
  int                   thumb_max;

  threads_mutex_t       lru_lock;
  dt_thumbnail_t       *lru;   // least recently used thumbnail, delete this first
  dt_thumbnail_t       *mru;   // most  recently used thumbnail, append here
}
dt_thumbnails_t;

// init a thumbnail cache
VkResult dt_thumbnails_init(
    dt_thumbnails_t *tn,     // struct to be constructed
    // to avoid demosaicing, you want at least 3x downsampling (x-trans):
    const int wd,            // max width of thumbnail
    const int ht,            // max height of thumbnail
    const int cnt,           // max number of thumbnails
    const size_t heap_size); // max heap size in bytes (allocated on GPU)

// free all resources
void dt_thumbnails_cleanup(dt_thumbnails_t *tn);

// should be run in a separate thread:
// precache bc1 thumbnails for a directory of raw images
VkResult dt_thumbnails_cache_directory(dt_thumbnails_t *tn, const char *dirname);

// load one bc1 thumbnail for a given filename. fills thumb_index and returns
// VK_SUCCESS if all went well.
VkResult dt_thumbnails_load_one(dt_thumbnails_t *tn, const char *filename, uint32_t *thumb_index);
