#pragma once
#include "pipe/global.h"
// simple resource location indirection management

#ifdef __ANDROID__
#include <stdio.h>
#include <errno.h>
#include <android/asset_manager.h>
#include "android_native_app_glue.h"

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
  snprintf(filename, sizeof(filename), "%s", fname);
  if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
  return android_fopen(filename, mode);
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
