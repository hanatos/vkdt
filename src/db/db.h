#pragma once

#include <stdint.h>
#include <string.h>
#include <strings.h>

typedef enum dt_image_label_t
{
  s_image_label_none   = 0,
  s_image_label_red    = 1<<0,
  s_image_label_green  = 1<<1,
  s_image_label_blue   = 1<<2,
  s_image_label_yellow = 1<<3,
  s_image_label_purple = 1<<4,
  s_image_label_selected = 1<<15,
}
dt_image_label_t;

typedef struct dt_image_t
{
  const char *filename;  // point into db.sp_filename.buf stringpool
  uint32_t    thumbnail; // index into thumbnails->thumb[] or -1u
  uint16_t    rating;    // -1u reject 0 1 2 3 4 5 stars
  uint16_t    labels;    // each bit is one colour label flag, 1<<15 is selected bit
}
dt_image_t;

// forward declare for stringpool.h so we don't have to include it here.
typedef struct dt_stringpool_entry_t dt_stringpool_entry_t;
typedef struct dt_stringpool_t
{
  uint32_t entry_max;
  dt_stringpool_entry_t *entry;

  uint32_t buf_max;
  uint32_t buf_cnt;
  char *buf;
}
dt_stringpool_t;

typedef enum dt_db_property_t
{
  s_prop_none        = 0, // select all
  s_prop_filename    = 1, // strstr
  s_prop_rating      = 2, // >=
  s_prop_labels      = 3, // &
  s_prop_createdate  = 4,
}
dt_db_property_t;

__attribute__((unused))
static const char*
dt_db_property_text =
{
  "none\0"
  "filename\0"
  "rating\0"
  "label\0"
  "create date\0"
  "\0"
};

typedef struct dt_db_t
{
  char dirname[1018];           // full path of currently opened directory
  char basedir[1024];           // db base dir, where are tags etc

  // list of images in database
  dt_image_t *image;
  uint32_t image_cnt;
  uint32_t image_max;

  // string pool for image file names
  dt_stringpool_t sp_filename;

  // TODO: light table edit history

  // current sort and filter criteria for collection
  dt_db_property_t collection_sort;
  dt_db_property_t collection_filter;
  uint64_t         collection_filter_val;

  // current query
  uint32_t *collection;
  uint32_t  collection_cnt;
  uint32_t  collection_max;

  // selection
  uint32_t *selection;
  uint32_t  selection_cnt;
  uint32_t  selection_max;

  // currently selected image (when switching to darkroom mode, e.g.)
  uint32_t current_imgid;
  uint32_t current_colid;
}
dt_db_t;

void dt_db_init   (dt_db_t *db);
void dt_db_cleanup(dt_db_t *db);

typedef struct dt_thumbnails_t dt_thumbnails_t;
void dt_db_load_directory(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *dirname);

int dt_db_load_image(
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
         !strcasecmp(f2, ".cr3") ||
         !strcasecmp(f2, ".crw") ||
         !strcasecmp(f2, ".nef") ||
         !strcasecmp(f2, ".raw") ||
         !strcasecmp(f2, ".tif") ||
         !strcasecmp(f2, ".orf") ||
         !strcasecmp(f2, ".arw") ||
         !strcasecmp(f2, ".srw") ||
         !strcasecmp(f2, ".nrw") ||
         !strcasecmp(f2, ".kc2") ||
         !strcasecmp(f2, ".dng") ||
         !strcasecmp(f2, ".raf") ||
         !strcasecmp(f2, ".rw2") ||
         !strcasecmp(f2, ".pfm") || // floating point dumps
         !strcasecmp(f2, ".jpg") || // jpg images
         !strcasecmp(f2, ".mlv") || // magic lantern raw video files
         !strcasecmp(f2, ".mov") || // videos
         !strcasecmp(f2, ".cfg");   // also accept config files directly (preferrably so)
}

// add image to the list of selected images, O(1).
void dt_db_selection_add   (dt_db_t *db, uint32_t coid);
// remove image from the list of selected images, O(N).
void dt_db_selection_remove(dt_db_t *db, uint32_t colid);
void dt_db_selection_clear(dt_db_t *db);
// returns 1 if image is selected, 0 if not
int  dt_db_selection_contains(dt_db_t *db, uint32_t colid);
// return sorted list of selected images
const uint32_t *dt_db_selection_get(dt_db_t *db);

// return current image id (i.e. index into the global database list in db->image[.])
uint32_t dt_db_current_imgid(dt_db_t *db);
// return current collection id (i.e. index into the collection list of imageids, db->collection[.])
uint32_t dt_db_current_colid(dt_db_t *db);

// work with lighttable history
// TODO: modify image rating w/ adding history
// TODO: modify image labels w/ adding history

// read and write db config in ascii
int dt_db_read (dt_db_t *db, const char *filename);
int dt_db_write(const dt_db_t *db, const char *filename, int append);

// fill full file name with directory and extension.
// return 0 on success, else the buffer was too small.
int dt_db_image_path(const dt_db_t *db, const uint32_t imgid, char *fn, uint32_t maxlen);

// add image to named collection
int dt_db_add_to_collection(const dt_db_t *db, const uint32_t imgid, const char *cname);
// after changing filter and sort criteria, update the collection array
void dt_db_update_collection(dt_db_t *db);
// remove selection from database. pass del=1 to physically delete from disk
void dt_db_remove_selected_images(dt_db_t *db, dt_thumbnails_t *th, const int del);
// sets the current image to given collection id. pass -1u to clear.
void dt_db_current_set(dt_db_t *db, uint32_t colid);
// duplicate selected images:
// this writes only new .cfg files next to the old ones, with _01 _02 etc suffixes added.
// the reason why it doesn't update the db is because there may be thumbnailing processes
// running that will need a restart and the collection management might need to resize the
// internal storage anyways. this is all done at once by calling dt_gui_switch_collection,
// which by separation of concerns also cares about the thumbnail creation (the db doesn't).
void dt_db_duplicate_selected_images(dt_db_t *db);
