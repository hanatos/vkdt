#pragma once

// fast special cased parallel transactional database with generic
// metadata search

// 1) create list with imageid + metadata (couple dimensions, say <=3)
// 2) morton code sort (parallel radix sort, similar to pbrt's lbvh)
//    3d morton code in 64 bits limits us to ~1M entries in the db (2^20)
// 3) offset list is kd-tree for search
// 4) query any range/sort order

// then create a vertex array object from it and copy over to GPU for display

// for 1) we need metadata as 20-bit index. needs a string pool/hashtable? look
// into concurrencykit?

// TODO: need to store list of loaded images
// TODO: move to collection/database
typedef struct dt_image_t
{
  char filename[2048];
  uint32_t thumbnail;  // index into thumbnails->thumb[] or -1u
}
dt_image_t;

typedef struct dt_db_t
{
  // list of images in database
  dt_image_t *image;
  uint32_t image_cnt;
  uint32_t image_max;

  // TODO: some sort criterion next to collection (or hidden in upper bits)

  // current query
  uint32_t *collection;
  uint32_t  collection_cnt;
  uint32_t  collection_max;
}
dt_db_t;

// TODO: load directory into image[], call cache_one() to load thumb[]/VkImage
// TODO: cache_one needs to grab lru and append to list after mru

