#include "db.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>

void
dt_db_init(db_t *db)
{
  memset(db, 0, sizeof(*db));
}

void
dt_db_cleanup(db_t *db)
{
}

// TODO: put in shared place so thumbnails can use it, too
// TODO: is pipe the right place for thumbnails at all?
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


void dt_db_load_directory(
    db_t       *db,
    const char *dirname)
{
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
    if(!accept_filename(ep->d_name)) continue;
    db->image_max++;
  }

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);
  db->image_cnt = 0;

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  rewinddir(dp);
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG) continue; // accept DT_LNK, too?
    if(!accept_filename(ep->d_name)) continue;
    snprintf(filename, sizeof(filename), "%s/%s", dirname, ep->d_name);

    const uint32_t imgid = db->image_cnt++;
    db->image[imgid].thumbnail = -1u;
    snprintf(db->image[imgid].filename, sizeof(db->image[imgid].filename),
        "%s", filename);
    uint32_t thumbid = -1u;
    if(dt_thumbnails_load_one(
          &vkdt.thumbnails,
          filename,
          &thumbid) != VK_SUCCESS) continue;

    db->image[imgid].thumbnail = thumbid;

    // TODO: in fact, continue with th=-1u and trigger a
    // TODO: thumbnail cache process in the background, try again later!
  }

  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
}

