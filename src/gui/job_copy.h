#pragma once
#include "core/fs.h"
#include "db/db.h"
#include <sys/types.h>
#include <dirent.h>

typedef struct dt_job_copy_t
{ // copy contents of a folder
  char src[PATH_MAX], dst[PATH_MAX];
  char *buf;
  char **name;
  uint32_t cnt;
  uint32_t move;  // set to non-zero to remove src after copy
  _Atomic uint32_t abort;
  _Atomic uint32_t state;
  int taskid;
} dt_job_copy_t;

static inline void
dt_job_copy_cleanup(void *arg)
{ // task is done, every thread will call this (but we put only one)
  dt_job_copy_t *j = (dt_job_copy_t *)arg;
  if(j->buf)  free(j->buf);
  if(j->name) free(j->name);
  j->name = 0;
  j->buf = 0;
  j->state = 2;
}

static inline void
dt_job_copy_work(uint32_t item, void *arg)
{
  dt_job_copy_t *j = (dt_job_copy_t *)arg;
  if(j->abort) return;
  char src[PATH_MAX], dst[PATH_MAX];
  snprintf(src, sizeof(src), "%s/%s", j->src, j->name[item]);
  snprintf(dst, sizeof(dst), "%s/%s", j->dst, j->name[item]);
  fprintf(stderr, "copying %s to %s\n", src, dst);
  if(fs_copy(dst, src)) j->abort = 2;
  else if(j->move) fs_delete(src);
  // copy .cfg too if we can find it
  snprintf(src, sizeof(src), "%s/%s.cfg", j->src, j->name[item]);
  snprintf(dst, sizeof(dst), "%s/%s.cfg", j->dst, j->name[item]);
  if(!fs_copy(dst, src) && j->move) fs_delete(src);
  glfwPostEmptyEvent(); // redraw status bar
}

static inline int
dt_job_copy(
    dt_job_copy_t *j,
    const char *dst, // destination directory
    const char *src) // source directory, or "vkdt.db.sel" to copy the current selection
{
  j->abort = 0;
  j->state = 1;
  snprintf(j->src, sizeof(j->src), "%.*s", (int)sizeof(j->src)-1, src);
  snprintf(j->dst, sizeof(j->dst), "%.*s", (int)sizeof(j->dst)-1, dst);
  fs_mkdir_p(j->dst, 0777); // try and potentially fail to create destination directory

  if(!strcmp(src, "vkdt.db.sel"))
  { // fill from selection instead of whole directory
    const uint32_t *sel = dt_db_selection_get(&vkdt.db);
    j->cnt = vkdt.db.selection_cnt;
    if(vkdt.db.selection_cnt == 0) { j->state = 2; return -1; }
    size_t bufsize = j->cnt*PATH_MAX;
    j->buf = malloc(sizeof(char)*bufsize);
    j->name = malloc(sizeof(char*)*j->cnt);
    j->cnt = 0;
    int pos = 0;
    for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
    { // filename without the .cfg, we copy both
      j->name[j->cnt++] = j->buf + pos;
      int off = snprintf(j->buf + pos, bufsize - pos, "%s", vkdt.db.image[sel[i]].filename);
      if(off <= 0) break;
      pos += off+1;
    }
    snprintf(j->src, sizeof(j->src), "%.*s", (int)sizeof(j->src)-1, vkdt.db.dirname);
  }
  else
  {
    DIR* dirp = opendir(j->src);
    j->cnt = 0;
    struct dirent *ent = 0;
    if(!dirp) { j->state = 2; return -1; }
    // first count valid entries
    while((ent = readdir(dirp)))
      if(!fs_isdir(j->src, ent))
        j->cnt++;
    if(!j->cnt) { j->state = 2; return -1; }
    rewinddir(dirp); // second pass actually record stuff
    size_t bufsize = j->cnt*PATH_MAX;
    j->buf = malloc(sizeof(char)*bufsize);
    j->name = malloc(sizeof(char*)*j->cnt);
    j->cnt = 0;
    int pos = 0;
    while((ent = readdir(dirp))) if(!fs_isdir(j->src, ent))
    {
      j->name[j->cnt++] = j->buf + pos;
      int off = snprintf(j->buf+pos, bufsize-pos, "%s", ent->d_name);
      if(off <= 0) break;
      pos += off+1;
    }
    closedir(dirp);
  }

  j->taskid = threads_task("copy", j->cnt, -1, j, dt_job_copy_work, dt_job_copy_cleanup);
  return j->taskid;
}
