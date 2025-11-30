#include "graph.h"
#include "module.h"
#include "res.h"
#include "core/log.h"

#ifdef __ANDROID__

int android_read(void* cookie, char* buf, int size) {
  return AAsset_read((AAsset*)cookie, buf, size);
}

int android_write(void* cookie, const char* buf, int size) {
  return EACCES; // can't provide write access to the apk
}

fpos_t android_seek(void* cookie, fpos_t offset, int whence) {
  return AAsset_seek((AAsset*)cookie, offset, whence);
}

int android_close(void* cookie) {
  AAsset_close((AAsset*)cookie);
  return 0;
}

FILE*
android_fopen(const char* fname, const char* mode)
{
  if(mode[0] == 'w') return 0;
  if(!dt_pipe.app || !dt_pipe.app->activity || !dt_pipe.app->activity->assetManager) return 0;
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
FILE*
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
  { // try the file name as is, but only for absolute paths
    strncpy(filename, fname, sizeof(filename)-1);
    if((c = strstr(filename, "%04d"))) memcpy(c, fstr, 4);
    FILE *f = fopen(filename, mode);  // absolute path or relative to current dir
    return f; // this was an absolute path, continuing the game is hopeless.
  }
  if(graph && graph->searchpath[0])
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
