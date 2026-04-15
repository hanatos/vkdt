#pragma once
#include "pipe/global.h"
#include "core/fs.h"
#include "db/stringpool.h"
#include <dirent.h>
#ifdef __ANDROID__
#include "modules/modlist.h"
#include <stdio.h>
#include <errno.h>
#include "android_native_app_glue.h"
#include <android/asset_manager.h>
#endif
// simple resource location indirection management

static inline void
dt_graph_alloc_external_resources(
    dt_graph_t *graph)
{
  graph->ext_files = (dt_stringpool_t *)malloc(sizeof(dt_stringpool_t));
  dt_stringpool_init(graph->ext_files, 20, 80);
}

static inline void
dt_graph_free_external_resources(
    dt_graph_t *graph)
{
  if(!graph->ext_files) return;
  dt_stringpool_cleanup(graph->ext_files);
  free(graph->ext_files);
  graph->ext_files = 0;
}

static inline void
dt_graph_print_external_resources(
    const dt_graph_t *graph)
{
  if(!graph || !graph->ext_files) return;
  dt_stringpool_t *sp = graph->ext_files;
  for(int i=0;i<sp->buf_max;)
  {
    int len = strlen(sp->buf+i);
    if(len)
    {
      uint32_t pos = dt_stringpool_get(sp, sp->buf+i, len, -1u, 0);
      if(pos != -1u)
        fprintf(stderr, "%s\n", sp->buf+i);
    }
    i += len+1; // eat terminating 0, too
  }
}

static inline int // return 0 on success
dt_graph_append_external_resource(
    const dt_graph_t *graph,
    const char       *filename)
{
  if(!graph || !graph->ext_files) return 0;
  dt_stringpool_t *sp = (dt_stringpool_t *)graph->ext_files;
  dt_stringpool_get(sp, filename, strlen(filename), 0, 0);
  return 0;
}

// open a file pointer, consider search paths specific to this graph
// if the path is relative, searches in this order:
// * graph basedir
// * homedir
// * global basedir/apk assets
#ifndef VKDT_DSO_BUILD
VKDT_API FILE*
dt_graph_open_resource(
    const dt_graph_t *graph,   // graph associated with the module
    uint32_t          frame,   // optional frame for timelapses, if fname contains "%04d"
    const char       *fname,   // file name template (basename contains exactly "%04d")
    const char       *mode);   // open mode "r" or "w" etc will be passed to fopen
#endif

// take file name param and start frame param and return located raw file name.
// returns non-zero on failure.
// for relative paths, this searches *only* the graph basedir.
static inline int
dt_graph_get_resource_filename(
    const dt_module_t *mod,
    const char        *fname,
    int                frame,
    char              *ret,
    size_t             ret_size)
{
  char tmp[2*PATH_MAX+10];
  
  if(fname[0] != '/' && fname[1] != ':') // relative paths
  {
    snprintf(tmp, sizeof(tmp), "%s/%s", mod->graph->searchpath, fname);
    snprintf(ret, ret_size, tmp, frame);
    FILE *f = fopen(ret, "rb");
    if(!f) return 1;
    dt_graph_append_external_resource(mod->graph, ret);
    fclose(f);
  }
  else
  { // absolute path:
    snprintf(ret, ret_size, fname, frame);
    FILE *f = fopen(ret, "rb");
    if(!f) return 1;
    dt_graph_append_external_resource(mod->graph, ret);
    fclose(f);
  }
  return 0;
}

#ifdef __ANDROID__
static int dt_res_mod = 0;
#endif
// open directory in either home or in basedir/apk
// note: for android, this is *not thread-safe*!
static inline void*
dt_res_opendir(const char *dirname, const int inbase)
{
  if(!dirname) return 0;
#ifdef __ANDROID__
  if(inbase)
  { // AAARRRGH
    // for modules/, return pointer to static int referencing dt_mod.
    // this is necessarily static because this way we can tell further down in the other methods
    // that dir is pointing there, so it has to be the modules directory
    if(!strcmp(dirname, "modules")) { dt_res_mod = 0; return &dt_res_mod; }
    return AAssetManager_openDir(dt_pipe.app->activity->assetManager, dirname);
  }
#endif
  char dn[PATH_MAX];
  int r = snprintf(dn, sizeof(dn), "%s/%s",
      inbase ? dt_pipe.basedir : dt_pipe.homedir, dirname);
  if(r >= sizeof(dn)) return 0;
  return opendir(dn);
}
static inline int
dt_res_closedir(void *dir, const int inbase)
{
#ifdef __ANDROID__
  if(inbase)
  {
    if(dir == &dt_res_mod) return 0;
    AAssetDir_close((AAssetDir*)dir);
    return 0;
  }
#endif
  return closedir((DIR*)dir);
}
static inline const char*
dt_res_next_basename(void *dir, const int inbase)
{
#ifdef __ANDROID__
  // god fucking damn the android native api. you can't enumerate directories.
  // all we get is fucking filenames, no subdirs.
  // the satanically fucked answer to this: hardcode list of modules
  // in a header compile time and only go through this list in some order,
  // in case that dir=="modules"
  if(inbase)
  {
    if(dir == &dt_res_mod) 
    {
      if(dt_res_mod < sizeof(dt_mod)/sizeof(dt_mod[0])) 
        return dt_mod[dt_res_mod++];
      else return 0;
    }
    return fs_basename((char*)AAssetDir_getNextFileName((AAssetDir*)dir));
  }
#endif
  struct dirent *dp = readdir((DIR*)dir);
  if(dp) return dp->d_name;
  return 0;
}
static inline void
dt_res_rewinddir(void *dir, const int inbase)
{
#ifdef __ANDROID__
  if(inbase)
  {
    if(dir == &dt_res_mod) 
    {
      dt_res_mod = 0;
      return;
    }
    return AAssetDir_rewind((AAssetDir*)dir);
  }
#endif
  return rewinddir((DIR*)dir);
}
