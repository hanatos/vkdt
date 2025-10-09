#include "db.h"
#include "thumbnails.h"
#include "core/log.h"
#include "core/fs.h"
#include "core/sort.h"
#include "pipe/graph-defaults.h"
#include "pipe/res.h"
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
#include <inttypes.h>
#include <regex.h>

void
dt_db_init(dt_db_t *db)
{
  memset(db, 0, sizeof(*db));
  db->current_imgid = -1u;
  db->current_colid = -1u;
  fs_mkdir_p(dt_pipe.homedir, 0755);
  // probably read preferences here from config file instead:
  db->collection_sort = s_prop_filename;
  threads_mutex_init(&db->image_mutex, 0);
  dt_stringpool_init(&db->timeoffset_model, 10, 30);
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
  threads_mutex_destroy(&db->image_mutex);
  dt_stringpool_cleanup(&db->timeoffset_model);
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

void
dt_db_read_createdate(const dt_db_t *db, uint32_t imgid, char createdate[20])
{
  char fn[PATH_MAX], f[PATH_MAX];
  dt_db_image_path(db, imgid, fn, sizeof(fn));
  fs_realpath(fn, f);
  size_t off = strnlen(f, sizeof(f));
  if(off > 4) f[off - 4] = 0;
  else f[off] = 0;
  if(off > 7 && f[off-7] == '_' && f[off-6] >= '0' && f[off-6] <= '9' && f[off-5] >= '0' && f[off-5] <= '9') 
    f[off-7] = 0; // for duplicates, for instance IMG_9999.CR2_01.cfg

  char model[32];
  model[0] = 0;
  dt_db_exif_mini(f, createdate, model, sizeof(model));

#ifndef _WIN64 // oh whatever. is this dead os really worth all the trouble?
  // now consider the model/time offset table stored in the vkdt.db folder structure
  uint32_t i = dt_stringpool_get((dt_stringpool_t*)&db->timeoffset_model, model, strlen(model), -1u, 0);
  i = CLAMP(i, 0, sizeof(db->timeoffset)/sizeof(db->timeoffset[0])-1);
  if(i == -1u) return;
  // fprintf(stderr, "time `%s` model `%s` \n", createdate, model);
  int64_t to = db->timeoffset[i];
  struct tm tm;
  strptime(createdate, "%Y:%m:%d %T", &tm);
  time_t t = timegm(&tm);
  t += to;
  gmtime_r(&t, &tm);
  strftime(createdate, 20, "%Y:%m:%d %T", &tm);
  // fprintf(stderr, "backconv - off %ld %s\n", to, createdate);
#endif
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
  dt_db_read_createdate(db, ia[0], cda);
  dt_db_read_createdate(db, ib[0], cdb);
  return strcmp(cda, cdb);
}

static int
compare_filetype(const void *a, const void *b, void *arg)
{
  dt_db_t *db = arg;
  const uint32_t *ia = a, *ib = b;
  dt_token_t ta = dt_graph_default_input_module(db->image[ia[0]].filename);
  dt_token_t tb = dt_graph_default_input_module(db->image[ib[0]].filename);
  // convert 64 to 32 bits:
  if(ta > tb) return 1;
  else if(tb > ta) return -1;
  return 0;
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
  regex_t regex;
  if(db->collection_filter.active & (1<<s_prop_filename))
    regcomp(&regex, db->collection_filter.filename, 0);
  char createdate[20];
  db->collection_cnt = 0;
  for(int k=0;k<db->image_cnt;k++)
  {
    for(int f=1;f<s_prop_cnt;f++)
    {
      if(db->collection_filter.active & (1<<f))
      {
        switch(f)
        {
        case s_prop_none:
          break;
        case s_prop_filename:
          if(regexec(&regex, db->image[k].filename, 0, 0, 0)) goto discard;
          break;
        case s_prop_rating:
          if(db->collection_filter.rating_cmp == 0 && !(db->image[k].rating >= db->collection_filter.rating)) goto discard;
          if(db->collection_filter.rating_cmp == 1 && !(db->image[k].rating == db->collection_filter.rating)) goto discard;
          if(db->collection_filter.rating_cmp == 2 && !(db->image[k].rating <  db->collection_filter.rating)) goto discard;
          break;
        case s_prop_labels:
          if(!(db->image[k].labels & db->collection_filter.labels)) goto discard;
          break;
        case s_prop_createdate:
          dt_db_read_createdate(db, k, createdate);
          if(!strstr(createdate, db->collection_filter.createdate)) goto discard;
          break;
        case s_prop_filetype:
          if(dt_graph_default_input_module(db->image[k].filename) != db->collection_filter.filetype) goto discard;
          break;
        }
      }
    }
    db->collection[db->collection_cnt++] = k;
discard:;
  }
  if(db->collection_filter.active & (1<<s_prop_filename))
    regfree(&regex);
  // TODO: use db/tests/parallel radix sort
  switch(db->collection_sort)
  {
  case s_prop_none: case s_prop_cnt:
    break;
  case s_prop_filename:
    sort(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_filename, db);
    break;
  case s_prop_rating:
    sort(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_rating, db);
    break;
  case s_prop_labels:
    sort(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_labels, db);
    break;
  case s_prop_createdate:
    sort(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_createdate, db);
    break;
  case s_prop_filetype:
    sort(db->collection, db->collection_cnt, sizeof(db->collection[0]), compare_filetype, db);
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
    if(!fs_isreg(dirname, ep) && !fs_islnk(dirname, ep)) continue;
    if(!dt_db_accept_filename(ep->d_name)) continue;
    db->image_max++;
  }

  db->image = malloc(sizeof(dt_image_t)*db->image_max);
  memset(db->image, 0, sizeof(dt_image_t)*db->image_max);

  db->collection_max = db->image_max;
  db->collection = malloc(sizeof(uint32_t)*db->collection_max);

  db->selection_max = db->image_max;
  db->selection = malloc(sizeof(uint32_t)*db->selection_max);

  // you would not believe how lengthy people name their files:
  dt_stringpool_init(&db->sp_filename, db->collection_max, 50);

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
    if(!fs_isreg(dirname, ep) && !fs_islnk(dirname, ep)) continue;
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

uint32_t dt_db_filename_colid(dt_db_t *db, const char *basename)
{
  for(uint32_t i=0;i<db->collection_cnt;i++)
    if(!strcmp(basename, db->image[db->collection[i]].filename)) return i;
  return -1u;
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
  switch(db->collection_sort)
  { // sync sorting criterion with collection
  case s_prop_none: case s_prop_cnt:
    break;
  case s_prop_filename:
    sort(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_filename, db);
    break;
  case s_prop_rating:
    sort(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_rating, db);
    break;
  case s_prop_labels:
    sort(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_labels, db);
    break;
  case s_prop_createdate:
    sort(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_createdate, db);
    break;
  case s_prop_filetype:
    sort(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_filetype, db);
    break;
  }
  return db->selection;
}

static inline int64_t
dt_db_parse_timeoffset(const char *str)
{
  int64_t Y, M, D, h, m, s;
  sscanf(str, "%"PRId64":%"PRId64":%"PRId64":%"PRId64":%"PRId64":%"PRId64, &Y, &M, &D, &h, &m, &s);
  // i hate calendars, and none of this makes any sense:
  return s + 60*(m + 60*(h + 24*(D + 30*M + 365*Y)));
}

int dt_db_read(dt_db_t *db, const char *filename)
{
  dt_stringpool_reset(&db->timeoffset_model);
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;
  char line[256];
  char imgn[256];
  char what[256], val[256];
  dt_db_filter_t *ft = &db->collection_filter;
  ft->active = 0;

  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read endline
    lno++;

    // scan filename:rating|labels:number
    sscanf(line, "%[^:]:%[^:]:%s", imgn, what, val);

    if(!strcmp(imgn, "sort"))
    {
      if(!strcmp(what, "filename"))    db->collection_sort = s_prop_filename;
      if(!strcmp(what, "rating"))      db->collection_sort = s_prop_rating;
      if(!strcmp(what, "label"))       db->collection_sort = s_prop_labels;
      if(!strcmp(what, "create date")) db->collection_sort = s_prop_createdate;
      if(!strcmp(what, "file type"))   db->collection_sort = s_prop_filetype;
      continue;
    }
    if(!strcmp(imgn, "filter"))
    {
      if(!strcmp(what, "filename"))    { ft->active |= 1<<s_prop_filename;   snprintf(ft->filename, sizeof(ft->filename), "%s", val); }
      if(!strcmp(what, "rating"))      { ft->active |= 1<<s_prop_rating;     ft->rating = atol(val); }
      if(!strcmp(what, "label"))       { ft->active |= 1<<s_prop_labels;     ft->labels = atol(val); }
      if(!strcmp(what, "create date")) { ft->active |= 1<<s_prop_createdate; snprintf(ft->createdate, sizeof(ft->createdate), "%s", val); }
      if(!strcmp(what, "file type"))   { ft->active |= 1<<s_prop_filetype;   strncpy(dt_token_str(ft->filetype), val, 8); }
      continue;
    }
    if(!strcmp(imgn, "time offset"))
    {
      int64_t to = dt_db_parse_timeoffset(val);
      uint32_t i = dt_stringpool_get(&db->timeoffset_model, what, strlen(what), db->timeoffset_cnt++, 0);
      i = CLAMP(i, 0, sizeof(db->timeoffset)/sizeof(db->timeoffset[0])-1);
      db->timeoffset[i] = to;
      continue;
    }

    // get image id or -1u, never insert, not interested in the string pointer:
    uint32_t imgid = dt_stringpool_get(&db->sp_filename, imgn, strlen(imgn), -1u, 0);
    if(imgid != -1u && imgid < db->image_cnt)
    {
      if     (!strcasecmp(what, "rating"))
        db->image[imgid].rating = atol(val);
      else if(!strcasecmp(what, "labels"))
        db->image[imgid].labels = atol(val);
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

  const dt_db_filter_t *ft = &db->collection_filter;
  if(ft->active & (1<<s_prop_filename))   fprintf(f, "filter:filename:%s\n",    ft->filename);
  if(ft->active & (1<<s_prop_rating))     fprintf(f, "filter:rating:%u\n",      ft->rating);
  if(ft->active & (1<<s_prop_labels))     fprintf(f, "filter:label:%u\n",       ft->labels);
  if(ft->active & (1<<s_prop_createdate)) fprintf(f, "filter:create date:%s\n", ft->createdate);
  if(ft->active & (1<<s_prop_filetype))   fprintf(f, "filter:file type:%s\n",   dt_token_str(ft->filetype));

  for(int i=0;i<db->timeoffset_model.buf_max;)
  {
    int len = strlen(db->timeoffset_model.buf+i);
    if(len)
    { // query value of this string and write time offset in seconds
      uint32_t pos = dt_stringpool_get((dt_stringpool_t*)&db->timeoffset_model, db->timeoffset_model.buf+i, len, -1u, 0);
      if(pos != -1u && pos < sizeof(db->timeoffset)/sizeof(db->timeoffset[0]))
        fprintf(f, "time offset:%s:0:0:0:0:0:%"PRId64"\n", db->timeoffset_model.buf+i, db->timeoffset[pos]);
    }
    i += len+1; // eat terminating 0, too
  }


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
// get image_path, get hash
// create directory ~/.config/vkdt/tags/<tag>/ and link <hash> to our image path
// use relative path names in link? still useful if ~/.config top level?
int dt_db_add_to_collection(const dt_db_t *db, const uint32_t imgid, const char *cname)
{
  char filename[PATH_MAX+100];
  dt_db_image_path(db, imgid, filename, sizeof(filename));

  uint64_t hash = hash64(filename);
  char dirname[1040];
  snprintf(dirname, sizeof(dirname), "%s/tags/%s", dt_pipe.homedir, cname);
  fs_mkdir_p(dirname, 0755); // ignore error, might exist already (which is fine)
  char linkname[1040];
  snprintf(linkname, sizeof(linkname), "%s/tags/%s/%"PRIx64".cfg", dt_pipe.homedir, cname, hash);
  int err = fs_symlink(filename, linkname);
  if(err) return 1;
  return 0;
}

void dt_db_remove_selected_images(
    dt_db_t *db,
    dt_thumbnails_t *thumbnails,
    const int del)
{
  // sort selection array by id:
  sort(db->selection, db->selection_cnt, sizeof(db->selection[0]), compare_id, db);

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

void dt_db_reset_to_defaults(
    const char *fn) // obtain full file name by calling dt_db_image_path, also resolve symlinks by using fs_realpath first
{
  char fullfn[PATH_MAX]; // store away full config path
  snprintf(fullfn, sizeof(fullfn), "%s", fn);
  int len = strlen(fullfn); // we'll work with the full file name, symlinks do not work
  assert(len > 4);
  fullfn[len-4] = 0; // cut away ".cfg"
  // cut away directory part and potential _XX duplicate id
  if(fullfn[len-7] == '_' && isdigit(fullfn[len-5]) && isdigit(fullfn[len-6]))
    fullfn[len-7] = 0;
  dt_token_t input_module = dt_graph_default_input_module(fullfn);
  char defcfg[PATH_MAX+30];
  snprintf(defcfg, sizeof(defcfg), "default-darkroom.%" PRItkn, dt_token_str(input_module));

  FILE *defcfgf = dt_graph_open_resource(0, 0, defcfg, "rb");
  if(!defcfgf)
  {
    dt_log(s_log_db|s_log_err, "could not copy default cfg for %"PRItkn, dt_token_str(input_module));
    return;
  }
  fs_copy_file(fn, defcfgf);
  fclose(defcfgf);
  FILE *f = fopen(fn, "ab");
  if(f)
  {
    char *c = fs_basename(fullfn);
    fprintf(f, "param:%"PRItkn":main:filename:%s\n", dt_token_str(input_module), c);
    fclose(f);
  }
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
    if(!dt_db_image_path(db, db->selection[i], fullfn, sizeof(fullfn)) && fs_copy(fn, fullfn))
    { // that went wrong, try to copy default config:
      dt_db_reset_to_defaults(fn);
    }
next:;
  }
}
