#pragma once
#include "pipe/global.h"
#include "core/fs.h"
#include <dirent.h>
// simple resource location indirection management

#ifdef __ANDROID__
#include <stdio.h>
#include <errno.h>
#include <android/asset_manager.h>
#include "android_native_app_glue.h"
#include "modules/modlist.h"

static int android_read(void* cookie, char* buf, int size) {
  return AAsset_read((AAsset*)cookie, buf, size);
}

static int android_write(void* cookie, const char* buf, int size) {
  return EACCES; // can't provide write access to the apk
}

static fpos_t android_seek(void* cookie, fpos_t offset, int whence) {
  return AAsset_seek((AAsset*)cookie, offset, whence);
}

static int android_close(void* cookie) {
  AAsset_close((AAsset*)cookie);
  return 0;
}

static inline FILE*
android_fopen(const char* fname, const char* mode)
{
  if(mode[0] == 'w') return 0;

  AAssetManager *ass = dt_pipe.app->activity->assetManager;
  AAsset* asset = AAssetManager_open(ass, fname, 0);
  if(!asset) return 0;

  return funopen(asset, android_read, android_write, android_seek, android_close);
}
#endif


// open a file pointer, consider search paths specific to this graph
// if the path is relative, searches in this order:
// * graph basedir
// * homedir
// * global basedir/apk assets
static inline FILE*
dt_graph_open_resource(
    const dt_graph_t *graph,   // graph associated with the module
    uint32_t          frame,   // optional frame for timelapses, if fname contains "%04d"
    const char       *fname,   // file name template (basename contains exactly "%04d")
    const char       *mode)    // open mode "r" or "w" etc will be passed to fopen
{
  char fstr[5] = {0}, *c = 0;
  snprintf(fstr, sizeof(fstr), "%04d", frame); // for security reasons don't use user-supplied fname as format string
  char filename[2*PATH_MAX+10];
#ifdef _WIN64
  if(fname[0] == '/' || fname[1] == ':')
#else
  if(fname[0] == '/')
#endif
  {
    strncpy(filename, fname, sizeof(filename)-1);
    if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
    return fopen(filename, mode);  // absolute path
  }
  if(graph)
  { // for relative paths, add search path
    snprintf(filename, sizeof(filename), "%s/%s", graph->searchpath, fname);
    if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
    FILE *f = fopen(filename, mode);
    if(f) return f;
  }
  // if we can't open it in the graph specific search path, try the home directory:
  snprintf(filename, sizeof(filename), "%s/%s", dt_pipe.homedir, fname);
  if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
  FILE *f = fopen(filename, mode);
  if(f) return f;
  // global basedir/apk
#ifdef __ANDROID__
  snprintf(filename, sizeof(filename), "%s", fname);
  if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
  return android_fopen(filename, mode);
#else
  snprintf(filename, sizeof(filename), "%s/%s", dt_pipe.basedir, fname);
  if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
  return fopen(filename, mode);
#endif
}

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

static int dt_res_mod = 0;
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
  // TODO:
  // do the satanically fucked answer to this: hardcode list of modules
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
  return dp->d_name;
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
