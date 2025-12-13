#pragma once
#include "pipe/global.h"
#include "core/fs.h"
#include <dirent.h>
#ifdef __ANDROID__
#include "modules/modlist.h"
#include <stdio.h>
#include <errno.h>
#include "android_native_app_glue.h"
#include <android/asset_manager.h>
#endif
// simple resource location indirection management

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
    fclose(f);
  }
  else
  { // absolute path:
    snprintf(ret, ret_size, fname, frame);
    FILE *f = fopen(ret, "rb");
    if(!f) return 1;
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
