#include "db.h"
#include "thumbnails.h"
#include "core/log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
  memset(db, 0, sizeof(*db));
}

static int
compare_filename(const void *a, const void *b, void *arg)
{
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  return strcmp(db->image[ia[0]].filename, db->image[ib[0]].filename);
}

void dt_db_load_directory(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *dirname)
{
  uint32_t id = -1u;
  dt_thumbnails_load_one(thumbnails, "data/busybee.bc1", &id);
  id = -1u;
  dt_thumbnails_load_one(thumbnails, "data/bomb.bc1", &id);

  DIR *dp = opendir(dirname);
  if(!dp)
  {
    dt_log(s_log_err|s_log_db, "could not open directory '%s'!", dirname);
    return;
  }
  struct dirent *ep;
  db->image_max = 0;
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG && ep->d_type != DT_LNK) continue;
    if(!dt_db_accept_filename(ep->d_name)) continue;
    db->image_max++;
  }

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  // the gui thread in main.c starts two background threads creating thumbnails, if needed.
  // thumbnails_load_list() will load the created bc1, triggered in render.cc
  rewinddir(dp);
  clock_t beg = clock();
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG && ep->d_type != DT_LNK) continue;
    if(!dt_db_accept_filename(ep->d_name)) continue;

    const uint32_t imgid = db->image_cnt++;
    db->image[imgid].thumbnail = 0; // loading icon
    snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename),
      "%s/%s", dirname, ep->d_name);

    // now reject non-cfg files that have a cfg already:
    char cfgfile[256];
    snprintf(cfgfile, sizeof(cfgfile), "%s", db->image[imgid].filename);
    int len = strlen(cfgfile);
    if(len <= 4) continue;
    char *f2 = cfgfile + len - 4;
    if(strcasecmp(f2, ".cfg"))
    { // not a cfg itself
      sprintf(f2+4, ".cfg"); // this would be the corresponding default cfg
      struct stat statbuf = {0};
      if(!stat(cfgfile, &statbuf))
      { // skip this image, it already has a cfg associated with it, we'll load that:
        db->image_cnt--;
        continue;
      }
      // no config associated with this image yet, let's use the default:
      snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename), "%s", cfgfile);
    }
  }
  clock_t end = clock();
  dt_log(s_log_perf|s_log_db, "time to load images %2.3fs", (end-beg)/(double)CLOCKS_PER_SEC);

  // TODO: use db/tests/parallel radix sort
  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
  // sort by filename:
  qsort_r(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_filename, db);
}

void dt_db_load_image(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *filename)
{
  if(!dt_db_accept_filename(filename)) return;
  db->image_max = 1;
  int len = strnlen(filename, 2048);
  int no_cfg = 0;
  if(len <= 4 || strcasecmp(filename+len-4, ".cfg")) no_cfg = 1;

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  db->image_cnt = 1;
  const uint32_t imgid = 0;
  db->image[imgid].thumbnail = -1u;
  if(no_cfg)
    snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename),
        "%s.cfg", filename);
  else
    snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename),
        "%s", filename);
  uint32_t thumbid = -1u;
  if(dt_thumbnails_load_one(
        thumbnails,
        db->image[imgid].filename,
        &thumbid) != VK_SUCCESS) return;

  db->image[imgid].thumbnail = thumbid;

  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
}
