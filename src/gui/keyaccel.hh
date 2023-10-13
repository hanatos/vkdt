#pragma once
#include "db/stringpool.h"
#include "core/log.h"
#include <dirent.h>

// this is responsible for managing (user supplied) tiny-presets that can be
// bound to a button via the hotkey system.
// it will read global and custom keyaccel preset files from data/keyaccel/*
// and ~/.config/vkdt/keyaccel/*, respectively.

typedef struct dt_keyaccel_t
{
  dt_stringpool_t sp;
}
dt_keyaccel_t;

inline int // return the new count after putting our stuff in there
dt_keyaccel_init(
    dt_keyaccel_t    *ka,        // key accel struct to initialise/store strings on
    ImHotKey::HotKey *list,      // list of partially inited hotkeys
    int               list_cnt,  // currently in list
    const int         list_size) // allocation size of the array
{
  dt_stringpool_init(&ka->sp, 2*list_size, 30); // + space for description/comment
  struct dirent **ent = 0;
  char dirname[PATH_MAX];
  uint32_t id = 0;
  for(int dir=0;dir<2;dir++)
  { // first search home dir, then system wide:
    size_t r = 0;
    if(dir) r = snprintf(dirname, sizeof(dirname), "%s/data/keyaccel", dt_pipe.basedir);
    else    r = snprintf(dirname, sizeof(dirname), "%s/keyaccel", vkdt.db.basedir);
    if(r >= sizeof(dirname)) continue; // truncated
    int ent_cnt = scandir(dirname, &ent, 0, alphasort);
    if(ent_cnt == -1) continue; // no such directory, we're out
    for(int i=0;i<ent_cnt;i++)
    {
      id++;
      if(list_cnt >= list_size) break;
      if(ent[i]->d_name[0] == '.') continue; // skip parent directories and hidden files

      char comment[256] = {0};
      char filename[PATH_MAX];
      size_t r = snprintf(filename, sizeof(filename), "%s/%s", dirname, ent[i]->d_name);
      if(r >= sizeof(filename)) continue; // truncated
      FILE *f = fopen(filename, "rb");
      if(f)
      { // try to read comment line
        fscanf(f, "# %255[^\n]", comment);
        fclose(f);
      }
      else continue; // we can't even open the file!

      const char *dedup0 = 0, *dedup1 = 0;
      uint32_t id0 = dt_stringpool_get(&ka->sp, ent[i]->d_name, strlen(ent[i]->d_name), id, &dedup0);
      if(id0 != id) continue; // this preset name already inserted (probably local overriding global)
      dt_stringpool_get(&ka->sp, comment, strlen(comment), 0, &dedup1);
      // dt_log(s_log_gui, "[keyaccel] adding hotkey %d %s %s\n", list_cnt, dedup0, dedup1);
      list[list_cnt].functionName = dedup0;
      list[list_cnt].functionLib  = dedup1;
      for(int k=0;k<4;k++) list[list_cnt].key[k] = 0; // these will be set when reading the darkroom.hotkeys config file
      list_cnt++;
    }
    for(int i=0;i<ent_cnt;i++) free(ent[i]);
    free(ent);
    ent = 0;
  }
  return list_cnt;
}

inline void
dt_keyaccel_cleanup(
    dt_keyaccel_t *ka)
{
  dt_stringpool_cleanup(&ka->sp);
}

inline void
dt_keyaccel_exec(const char *key)
{ // first search home dir, then global dir. ingest preset line by line, with history
  char filename[PATH_MAX];
  snprintf(filename, sizeof(filename), "%s/keyaccel/%s", vkdt.db.basedir, key);
  FILE *f = fopen(filename, "rb");
  if(!f)
  {
    size_t r = snprintf(filename, sizeof(filename), "%s/data/keyaccel/%s", dt_pipe.basedir, key);
    if(r >= sizeof(filename)) return; // truncated
    f = fopen(filename, "rb");
  }
  if(!f) return; // out of luck today
  uint32_t lno = 0;
  char line[300000];
  while(!feof(f))
  {
    fscanf(f, "%299999[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    lno++;
    // > 0 are warnings, < 0 are fatal, 0 is success
    if(dt_graph_read_config_line(&vkdt.graph_dev, line) < 0) goto error;
    dt_graph_history_line(&vkdt.graph_dev, line);
  }
  if(0)
  {
error:
    dt_log(s_log_err, "[keyaccel] failed to execute %s : %d", key, lno);
  }
  vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
  fclose(f);
}
