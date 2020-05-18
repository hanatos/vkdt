#include "db.h"
#include "thumbnails.h"
#include "core/log.h"
#include "stringpool.h"

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
  db->current_imgid = -1u;
  db->current_colid = -1u;
}

void
dt_db_cleanup(dt_db_t *db)
{
  char dbname[256];
  snprintf(dbname, sizeof(dbname), "%s/vkdt.db", db->dirname);
  // do not write if opened single image:
  if(db->image_cnt > 1) dt_db_write(db, dbname, 0);
  dt_stringpool_cleanup(&db->sp_filename);
  memset(db, 0, sizeof(*db));
}

static int
compare_filename(const void *a, const void *b, void *arg)
{
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  return strcmp(db->image[ia[0]].filename, db->image[ib[0]].filename);
}

static inline void
image_init(dt_image_t *img)
{
  img->thumbnail = 0; // loading icon
  img->filename = 0;
  img->rating = 0;
  img->labels = 0;
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

  db->selection_max = db->image_max;
  db->selection = malloc(sizeof(uint32_t)*db->selection_max);

  dt_stringpool_init(&db->sp_filename, db->collection_max, 20);

  // the gui thread in main.c starts two background threads creating thumbnails, if needed.
  // thumbnails_load_list() will load the created bc1, triggered in render.cc
  rewinddir(dp);
  clock_t beg = clock();
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG && ep->d_type != DT_LNK) continue;
    if(!dt_db_accept_filename(ep->d_name)) continue;

    const uint32_t imgid = db->image_cnt++;
    image_init(db->image + imgid);

    // now reject non-cfg files that have a cfg already:
    char cfgfile[256];
    int ep_len = strlen(ep->d_name);
    if(ep_len > 4)
    {
      snprintf(cfgfile, sizeof(cfgfile), "%s/%s", dirname, ep->d_name);
      int len = strlen(cfgfile);
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
      }
      else ep_len -= 4; // remove '.cfg' suffix
    }

    // add base filename to string pool
    if(dt_stringpool_get(&db->sp_filename, ep->d_name, ep_len, imgid, &db->image[imgid].filename) == -1u)
    {
      dt_log(s_log_err|s_log_db, "failed to add filename to index! aborting import.");
      db->image_cnt--;
      break; // no use trying again
    }
  }
  clock_t end = clock();
  dt_log(s_log_perf|s_log_db, "time to load images %2.3fs", (end-beg)/(double)CLOCKS_PER_SEC);

  snprintf(db->dirname, sizeof(db->dirname), "%s", dirname);
  
  char dbname[256];
  snprintf(dbname, sizeof(dbname), "%s/vkdt.db", dirname);
  dt_db_read(db, dbname);

  // TODO: use db/tests/parallel radix sort
  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
  // sort by filename:
  qsort_r(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_filename, db);
}

int dt_db_load_image(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *filename)
{
  if(!dt_db_accept_filename(filename)) return 1;
  db->image_max = 1;
  int len = strnlen(filename, 2048);
  int no_cfg = 0;
  if(len <= 4 || strcasecmp(filename+len-4, ".cfg")) no_cfg = 1;
  else len -= 4; // remove .cfg suffix

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  db->selection_max = db->image_max;
  db->selection = malloc(sizeof(uint32_t)*db->selection_max);

  dt_stringpool_init(&db->sp_filename, db->collection_max, 256);

  db->image_cnt = 1;
  const uint32_t imgid = 0;
  image_init(db->image + imgid);
  db->image[imgid].thumbnail = -1u;
  db->dirname[0] = 0;

  if(dt_stringpool_get(&db->sp_filename, filename, len, imgid, &db->image[imgid].filename) == -1u)
  { // should never happen for a single image:
    dt_log(s_log_err|s_log_db, "failed to add filename to index! aborting import.");
    db->image_cnt--;
    return 1;
  }
  char fullfn[1024];
  dt_db_image_path(db, 0, fullfn, sizeof(fullfn));
  uint32_t thumbid = -1u;
  dt_thumbnails_load_one(
        thumbnails,
        fullfn,
        &thumbid); // nothing we can do if this fails

  db->image[imgid].thumbnail = thumbid;

  // collect all images: // TODO: abstract more
  db->collection_cnt = db->image_cnt;
  for(int k=0;k<db->collection_cnt;k++)
    db->collection[k] = k;
  return 0;
}

uint32_t dt_db_current_imgid(dt_db_t *db)
{
  return db->current_imgid;
}

uint32_t dt_db_current_colid(dt_db_t *db)
{
  return db->current_colid;
}

void dt_db_selection_add(dt_db_t *db, uint32_t colid)
{
  const uint32_t imgid = db->collection[colid];
  if(db->selection_cnt >= db->selection_max) return;
  int i = db->selection_cnt++;
  db->selection[i] = imgid;
  db->image[imgid].labels |= s_image_label_selected;
  db->current_imgid = imgid;
  db->current_colid = colid;
}

void dt_db_selection_remove(dt_db_t *db, uint32_t colid)
{
  if(db->current_colid == colid)
  {
    db->current_imgid = -1u;
    db->current_colid = -1u;
  }
  const uint32_t imgid = db->collection[colid];
  for(int i=0;i<db->selection_cnt;i++)
  {
    if(db->selection[i] == imgid)
    {
      db->selection[i] = db->selection[--db->selection_cnt];
      db->image[imgid].labels &= ~s_image_label_selected;
      return;
    }
  }
}

void dt_db_selection_clear(dt_db_t *db)
{
  for(int i=0;i<db->selection_cnt;i++)
  {
    const uint32_t imgid = db->selection[i];
    db->image[imgid].labels &= ~s_image_label_selected;
    // db->selection[i] = -1u; maybe less memory access is faster
  }
  db->selection_cnt = 0;
  db->current_imgid = -1u;
  db->current_colid = -1u;
}

const uint32_t *dt_db_selection_get(dt_db_t *db)
{
  // TODO: sync sorting criterion with collection
  qsort_r(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_filename, db);
  return db->selection;
}

int dt_db_read(dt_db_t *db, const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;
  char line[256];
  char imgn[256];
  char what[256];
  uint32_t num;

  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read endline
    lno++;

    // scan filename:rating|labels:number
    sscanf(line, "%[^:]:%[^:]:%d", imgn, what, &num);
    // get image id or -1u, never insert, not interested in the string pointer:
    uint32_t imgid = dt_stringpool_get(&db->sp_filename, imgn, strlen(imgn), -1u, 0);
    if(imgid != -1u && imgid < db->image_cnt)
    {
      if     (!strcasecmp(what, "rating"))
        db->image[imgid].rating = num;
      else if(!strcasecmp(what, "labels"))
        db->image[imgid].labels = num;
      else
        dt_log(s_log_db|s_log_err, "no such property in line %u: '%s'", lno, line);
    }
    else
      dt_log(s_log_db|s_log_err, "no such image in line %u: '%s'", lno, line);
  }
  fclose(f);
  return 0;
}

int dt_db_write(const dt_db_t *db, const char *filename, int append)
{
  FILE *f = append ? fopen(filename, "a+b") : fopen(filename, "wb");
  if(!f) return 1;
  for(int i=0;i<db->image_cnt;i++)
  {
    fprintf(f, "%s:rating:%u\n", db->image[i].filename, db->image[i].rating);
    fprintf(f, "%s:labels:%u\n", db->image[i].filename, db->image[i].labels & 0x7fffu); // mask selection bit
  }
  fclose(f);
  return 0;
}

int dt_db_image_path(const dt_db_t *db, const uint32_t imgid, char *fn, uint32_t maxlen)
{
  return snprintf(fn, maxlen, "%s/%s.cfg", db->dirname, db->image[imgid].filename) >= maxlen;
}

// TODO: add image to collection
// TODO: get image_path, get murmur3 hash
// TODO: create directory ~/.config/vkdt/tags/<tag>/ and link <hash> to our image path
// TODO: use relative path names in link? still useful if ~/.config top level?
