#pragma once

#include <stdint.h>
#include <string.h>
#include <strings.h>

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

  // currently selected image (when switching to darkroom mode, e.g.)
  uint32_t current_image;
}
dt_db_t;

void dt_db_init   (dt_db_t *db);
void dt_db_cleanup(dt_db_t *db);

typedef struct dt_thumbnails_t dt_thumbnails_t;
void dt_db_load_directory(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *dirname);

void dt_db_load_image(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *filename);

static inline int
dt_db_accept_filename(
    const char *f)
{
  // TODO: magic number checks instead.
  const char *f2 = f + strlen(f);
  while(f2 > f && *f2 != '.') f2--;
  return !strcasecmp(f2, ".cr2") ||
         !strcasecmp(f2, ".nef") ||
         !strcasecmp(f2, ".orf") ||
         !strcasecmp(f2, ".arw") ||
         !strcasecmp(f2, ".dng") ||
         !strcasecmp(f2, ".raf") ||
         !strcasecmp(f2, ".rw2") ||
         !strcasecmp(f2, ".cfg");   // also accept config files directly (preferrably so)
}
