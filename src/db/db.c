#include "db.h"
#include "thumbnails.h"
#include "core/log.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void
dt_db_init(dt_db_t *db)
{
  memset(db, 0, sizeof(*db));
  db->current_image = -1u;
}

void
dt_db_cleanup(dt_db_t *db)
{
}

void dt_db_load_directory(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *dirname)
{
  uint32_t id;
  dt_thumbnails_load_one(thumbnails, "../doc/busybee", &id);
  dt_thumbnails_load_one(thumbnails, "../doc/bomb", &id);

  DIR *dp = opendir(dirname);
  if(!dp)
  {
    dt_log(s_log_err|s_log_db, "could not open directory '%s'!", dirname);
    return;
  }
  struct dirent *ep;
  char filename[2048];
  db->image_max = 0;
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG) continue; // accept DT_LNK, too?
    if(!dt_db_accept_filename(ep->d_name)) continue;
    db->image_max++;
  }

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);
  db->image_cnt = 0;

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  rewinddir(dp);
  clock_t beg = clock();
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG) continue; // accept DT_LNK, too?
    if(!dt_db_accept_filename(ep->d_name)) continue;
    snprintf(filename, sizeof(filename), "%s/%s", dirname, ep->d_name);

    const uint32_t imgid = db->image_cnt++;
    db->image[imgid].thumbnail = 0; // loading icon
    snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename),
        "%s", filename);

    // TODO: dispatch in background thread:
    uint32_t thumbid;
    if(dt_thumbnails_load_one(
          thumbnails,
          filename,
          &thumbid) != VK_SUCCESS)
      thumbid = 1; // broken icon

    db->image[imgid].thumbnail = thumbid;
  }
  clock_t end = clock();
  dt_log(s_log_perf|s_log_db, "time to load thumbnails %2.3fs", (end-beg)/(double)CLOCKS_PER_SEC);

  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
}

void dt_db_load_image(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *filename)
{
  if(!dt_db_accept_filename(filename)) return;
  db->image_max = 1;

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  db->image_cnt = 1;
  const uint32_t imgid = 0;
  db->image[imgid].thumbnail = -1u;
  snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename),
      "%s", filename);
  uint32_t thumbid = -1u;
  if(dt_thumbnails_load_one(
        thumbnails,
        filename,
        &thumbid) != VK_SUCCESS) return;

  db->image[imgid].thumbnail = thumbid;

  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
}
