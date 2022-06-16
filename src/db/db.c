#include "db.h"
#include "thumbnails.h"
#include "core/log.h"
#include "core/fs.h"
#include "stringpool.h"
#include "exif.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

void
dt_db_init(dt_db_t *db)
{
  memset(db, 0, sizeof(*db));
  db->current_imgid = -1u;
  db->current_colid = -1u;
  snprintf(db->basedir, sizeof(db->basedir), "%s/.config/vkdt", getenv("HOME"));
  mkdir(db->basedir, 0755);
  // probably read preferences here from config file instead:
  db->collection_sort = s_prop_filename;
}

void
dt_db_cleanup(dt_db_t *db)
{
  char dbname[1040];
  snprintf(dbname, sizeof(dbname), "%s/vkdt.db", db->dirname);
  // do not write if opened single image:
  if(db->image_cnt > 1) dt_db_write(db, dbname, 0);
  dt_stringpool_cleanup(&db->sp_filename);
  free(db->collection);
  free(db->selection);
  free(db->image);
  memset(db, 0, sizeof(*db));
}

static int
compare_id(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return ia[0] - ib[0];
}

static int
compare_filename(const void *a, const void *b, void *arg)
{
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  return strcmp(db->image[ia[0]].filename, db->image[ib[0]].filename);
}

static int
compare_rating(const void *a, const void *b, void *arg)
{
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  // higher rating comes first:
  return db->image[ib[0]].rating - db->image[ia[0]].rating;
}

static int
compare_labels(const void *a, const void *b, void *arg)
{
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  return db->image[ia[0]].labels - db->image[ib[0]].labels;
}

static int
compare_createdate(const void *a, const void *b, void *arg)
{ // you ask for complicated sort, you get slow and stupid.
  // we don't store the date in the db, since there is in general no clear 1:1 mapping
  // between images and cfg file/graph. also, loading a directory becomes painfully slow
  // when initialising metadata during ingestion.
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  char cda[20] = {0}, cdb[20] = {0};
  char fna[1024], fnb[1024], *aa = fna, *bb = fnb;
  char fna2[1024], fnb2[1024];
  dt_db_image_path(db, ia[0], fna, sizeof(fna));
  dt_db_image_path(db, ib[0], fnb, sizeof(fnb));
  size_t off;
  off = readlink(fna, fna2, sizeof(fna2));
  if(off != -1) aa = fna2;
  else off = strnlen(fna, sizeof(fna));
  if(off > 4) aa[off - 4] = 0;
  else aa[off] = 0;

  off = readlink(fnb, fnb2, sizeof(fnb2));
  if(off != -1) bb = fnb2;
  else off = strnlen(fnb, sizeof(fnb));
  if(off > 4) bb[off - 4] = 0;
  else bb[off] = 0;

  char model[32];
  dt_db_exif_mini(aa, cda, model, sizeof(model));
  dt_db_exif_mini(bb, cdb, model, sizeof(model));

  return strcmp(cda, cdb);
}

static inline void
image_init(dt_image_t *img)
{
  // img->thumbnail = 0; // loading icon
  memset(img, 0, sizeof(*img));
}

void
dt_db_update_collection(dt_db_t *db)
{
  // filter
  db->collection_cnt = 0;
  for(int k=0;k<db->image_cnt;k++)
  {
    switch(db->collection_filter)
    {
    case s_prop_none:
      break;
    case s_prop_filename:
      if(!strstr(db->image[k].filename, dt_token_str(db->collection_filter_val))) continue;
      break;
    case s_prop_rating:
      if(!(db->image[k].rating >= db->collection_filter_val)) continue;
      break;
    case s_prop_labels:
      if(!(db->image[k].labels & db->collection_filter_val)) continue;
      break;
    case s_prop_createdate:
      // TODO: match beginning of filter val string
      break;
    }
    db->collection[db->collection_cnt++] = k;
  }
  // TODO: use db/tests/parallel radix sort
  switch(db->collection_sort)
  {
  case s_prop_none:
    break;
  case s_prop_filename:
    qsort_r(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_filename, db);
    break;
  case s_prop_rating:
    qsort_r(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_rating, db);
    break;
  case s_prop_labels:
    qsort_r(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_labels, db);
    break;
  case s_prop_createdate:
    qsort_r(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_createdate, db);
    break;
  }
}

void dt_db_load_directory(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *dirname)
{
  uint32_t id = 0;
  if(dt_thumbnails_load_one(thumbnails, "data/busybee.bc1", &id) != VK_SUCCESS)
  {
    dt_log(s_log_err|s_log_db, "could not load required thumbnail symbols!");
    return;
  }

  DIR *dp = dirname ? opendir(dirname) : 0;
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

  snprintf(db->dirname, sizeof(db->dirname), "%s", dirname);
  char *c = db->dirname + strlen(db->dirname) - 1;
  if(*c == '/') *c = 0; // remove trailing '/'

  // the gui thread in main.c starts two background threads creating thumbnails, if needed.
  // thumbnails_load_list() will load the created bc1, triggered in render.cc
  rewinddir(dp);
  char cfgfile[1500];
  clock_t beg = clock();
  while((ep = readdir(dp)))
  {
    if(ep->d_type != DT_REG && ep->d_type != DT_LNK) continue;
    if(!dt_db_accept_filename(ep->d_name)) continue;

    const uint32_t imgid = db->image_cnt++;
    image_init(db->image + imgid);

    // now reject non-cfg files that have a cfg already:
    int ep_len = strlen(ep->d_name);
    if(ep_len > 4)
    {
      snprintf(cfgfile, sizeof(cfgfile), "%s/%s", db->dirname, ep->d_name);
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

  char dbname[256];
  snprintf(dbname, sizeof(dbname), "%s/vkdt.db", dirname);
  dt_db_read(db, dbname);

  dt_db_update_collection(db);
}

int dt_db_load_image(
    dt_db_t         *db,
    dt_thumbnails_t *thumbnails,
    const char      *filename)
{
  if(!dt_db_accept_filename(filename)) return 1;
  db->image_max = 1;
  int len = strnlen(filename, 2048);
  if(len > 4 && !strcasecmp(filename+len-4, ".cfg"))
    len -= 4; // remove .cfg suffix

  uint32_t id = 0;
  if(dt_thumbnails_load_one(thumbnails, "data/busybee.bc1", &id) != VK_SUCCESS)
  {
    dt_log(s_log_err|s_log_db, "could not load required thumbnail symbols!");
    return 1;
  }

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

  // collect images:
  dt_db_update_collection(db);
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

void dt_db_current_set(dt_db_t *db, uint32_t colid)
{
  if(colid == -1u)
  { // clear
    db->current_imgid = -1u;
    db->current_colid = -1u;
    return;
  }
  if(colid >= db->collection_cnt) return;
  const uint32_t imgid = db->collection[colid];
  db->current_imgid = imgid;
  db->current_colid = colid;
}

void dt_db_selection_add(dt_db_t *db, uint32_t colid)
{
  if(colid >= db->collection_cnt) return;
  const uint32_t imgid = db->collection[colid];
  db->current_imgid = imgid; // always make current, even if selection is full
  db->current_colid = colid;
  if(db->selection_cnt >= db->selection_max) return;
  int i = db->selection_cnt++;
  db->selection[i] = imgid;
  db->image[imgid].labels |= s_image_label_selected;
}

int dt_db_selection_contains(dt_db_t *db, uint32_t colid)
{
  if(colid > db->collection_cnt) return 0;
  const uint32_t imgid = db->collection[colid];
  if(imgid > db->image_cnt) return 0;
  return (db->image[imgid].labels & s_image_label_selected) ? 1 : 0;
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
  uint64_t num;

  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read endline
    lno++;

    // scan filename:rating|labels:number
    sscanf(line, "%[^:]:%[^:]:%lu", imgn, what, &num);

    if(!strcmp(imgn, "sort"))
    {
      if(!strcmp(what, "filename"))    db->collection_sort = s_prop_filename;
      if(!strcmp(what, "rating"))      db->collection_sort = s_prop_rating;
      if(!strcmp(what, "label"))       db->collection_sort = s_prop_labels;
      if(!strcmp(what, "create date")) db->collection_sort = s_prop_createdate;
      continue;
    }
    if(!strcmp(imgn, "filter"))
    {
      if(!strcmp(what, "filename"))    db->collection_filter = s_prop_filename;
      if(!strcmp(what, "rating"))      db->collection_filter = s_prop_rating;
      if(!strcmp(what, "label"))       db->collection_filter = s_prop_labels;
      if(!strcmp(what, "create date")) db->collection_filter = s_prop_createdate;
      db->collection_filter_val = num;
      continue;
    }

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
  const char *c = dt_db_property_text;
  for(int i=0;i<db->collection_sort;i++,c++) while(*c) c++;
  fprintf(f, "sort:%s:\n", c);
  c = dt_db_property_text;
  for(int i=0;i<db->collection_filter;i++,c++) while(*c) c++;
  fprintf(f, "filter:%s:%lu\n", c, db->collection_filter_val);
  for(int i=0;i<db->image_cnt;i++)
  {
    if( db->image[i].rating          > 0) fprintf(f, "%s:rating:%u\n", db->image[i].filename, db->image[i].rating);
    if((db->image[i].labels&0x7fffu) > 0) fprintf(f, "%s:labels:%u\n", db->image[i].filename, db->image[i].labels & 0x7fffu); // mask selection bit
  }
  fclose(f);
  return 0;
}

int dt_db_image_path(const dt_db_t *db, const uint32_t imgid, char *fn, uint32_t maxlen)
{
  if(db->dirname[0])
    return snprintf(fn, maxlen, "%s/%s.cfg", db->dirname, db->image[imgid].filename) >= maxlen;
  else
    return snprintf(fn, maxlen, "%s.cfg", db->image[imgid].filename) >= maxlen;
}

// add image to named collection/tag
// get image_path, get murmur3 hash
// create directory ~/.config/vkdt/tags/<tag>/ and link <hash> to our image path
// use relative path names in link? still useful if ~/.config top level?
int dt_db_add_to_collection(const dt_db_t *db, const uint32_t imgid, const char *cname)
{
  char filename[PATH_MAX+100];
  dt_db_image_path(db, imgid, filename, sizeof(filename));

  uint32_t hash = murmur_hash3(filename, strlen(filename), 1337);
  char dirname[1040];
  snprintf(dirname, sizeof(dirname), "%s/tags", db->basedir);
  mkdir(dirname, 0755);
  snprintf(dirname, sizeof(dirname), "%s/tags/%s", db->basedir, cname);
  mkdir(dirname, 0755); // ignore error, might exist already (which is fine)
  char linkname[1040];
  snprintf(linkname, sizeof(linkname), "%s/tags/%s/%x.cfg", db->basedir, cname, hash);
  int err = symlink(filename, linkname);
  if(err) return 1;
  return 0;
}

void dt_db_remove_selected_images(
    dt_db_t *db,
    dt_thumbnails_t *thumbnails,
    const int del)
{
  // sort selection array by id:
  qsort_r(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_id, db);

  // go through sorted list of imgid, largest id first:
  char fullfn[2048] = {0};
  for(int i=db->selection_cnt-1;i>=0;i--)
  {
    if(del && !dt_db_image_path(db, db->selection[i], fullfn, sizeof(fullfn)))
    { // delete the cfg if any
      dt_log(s_log_db, "deleting `%s'", fullfn);
      unlink(fullfn);
      // delete the file without .cfg postfix
      size_t len = strnlen(fullfn, sizeof(fullfn));
      fullfn[len-4] = 0;
      dt_log(s_log_db, "deleting `%s'", fullfn);
      unlink(fullfn);
    }
    // swap this imgid with the last one in the db.
    // this will keep the db img list untouched up to here, so we can keep on doing this.
    const int gone = db->selection[i];
    const int keep = --db->image_cnt;
    const int gone_th = db->image[gone].thumbnail;
    const int keep_th = db->image[keep].thumbnail;
    db->image[gone].thumbnail = -1u;
    if(gone_th != -1u) thumbnails->thumb[gone_th].imgid = -1u;
    if(gone == keep) continue;
    if(keep_th != -1u) thumbnails->thumb[keep_th].imgid = gone;
    db->image[gone] = db->image[keep];
    db->image[keep].thumbnail = keep_th;
  }

  // select none:
  db->selection_cnt = 0;
  // freshly filter and sort collection
  dt_db_update_collection(db);
}

void dt_db_duplicate_selected_images(dt_db_t *db)
{
  char fn[PATH_MAX+200], fullfn[PATH_MAX];
  for(int i=0;i<db->selection_cnt;i++)
  {
    struct stat sb;
    const char *ifn = db->image[db->selection[i]].filename;
    int len = strnlen(ifn, 2048);
    if(len > 3 && ifn[len-3] == '_' && isdigit(ifn[len-2]) && isdigit(ifn[len-1]))
      len -= 3; // remove _?? suffix

    for(int k=1;k<100;k++)
    { // now append new index and probe until that file doesn't exist
      int err = 0;
      if(db->dirname[0])
        err = snprintf(fn, sizeof(fn), "%s/%.*s_%02d.cfg", db->dirname, len, ifn, k) >= sizeof(fn);
      else
        err = snprintf(fn, sizeof(fn), "%.*s_%02d.cfg", len, ifn, k) >= sizeof(fn);
      if(err) goto next;
      if(stat(fn, &sb) == -1) break; // found empty slot
      if(k == 99) goto next; // give up
    }
    dt_log(s_log_db, "creating duplicate `%s'", fn);
    if(!dt_db_image_path(db, db->selection[i], fullfn, sizeof(fullfn)))
      fs_copy(fn, fullfn);
next:;
  }
}
