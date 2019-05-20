#include "global.h"
#include "io.h"
#include "module.h"
#include "graph.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>

dt_pipe_global_t dt_pipe;

// reads one line of a connector configuration file.
// this is: connector name, type, channels, format
// as four tokens with ':' as separator.
static inline int
read_connector_ascii(
    dt_connector_t *conn,
    char *line)
{ // read tkn:tkn:tkn:tkn
  memset(conn, 0, sizeof(*conn));
  conn->name = dt_read_token(line, &line);
  conn->type = dt_read_token(line, &line);
  conn->chan = dt_read_token(line, &line);
  conn->format = dt_read_token(line, &line);
  return 0;
}

// read param config and default values and gui annotations
static inline dt_ui_param_t*
read_param_config_ascii(
    char *line)
{
  // params come in this format:
  // tkn:tkn:int:float:float:float
  // name:type:cnt:defval:min:max
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t type = dt_read_token(line, &line);
  int cnt = dt_read_int(line, &line);
  // TODO: sanity check and clamp?
  dt_ui_param_t *p = malloc(sizeof(*p) + cnt*3*sizeof(float));
  p->name = name;
  p->type = type;
  p->cnt = cnt;
  float *val = (float*)(p + 1);
  for(int i=0;i<p->cnt;i++)
  {
    *(val++) = dt_read_float(line, &line); // default
    *(val++) = dt_read_float(line, &line); // min
    *(val++) = dt_read_float(line, &line); // max
  }
  return p;
}

// default callback
void
modify_roi_out(dt_graph_t *graph, dt_module_t *module)
{
  // copy over roi from connector named "input" to all outputs ("write" or "sink")
  int input = dt_module_get_connector(module, dt_token("input"));
  dt_connector_t *c = module->connector+input;
  dt_roi_t *roi = &graph->module[c->connected_mid].connector[c->connected_cid].roi;
  for(int i=0;i<module->num_connectors;i++)
  {
    if(module->connector[i].type == dt_token("write") ||
       module->connector[i].type == dt_token("sink"))
    {
      module->connector[i].roi.full_wd = roi->full_wd;
      module->connector[i].roi.full_ht = roi->full_ht;
    }
  }
}

// default callback
void
modify_roi_in(dt_graph_t *graph, dt_module_t *module)
{
}

static inline int
dt_module_so_load(
    dt_module_so_t *mod,
    const char *dirname)
{
  memset(mod, 0, sizeof(*mod));
  mod->name = dt_token(dirname);
  char filename[2048], line[2048];
  // TODO: prepend search path, also below in similar places
  snprintf(filename, sizeof(filename), "modules/%s/lib%s.so", dirname, dirname);
  mod->dlhandle = 0;
  struct stat statbuf;
  if(!stat(filename, &statbuf))
  {
    mod->dlhandle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    if(dlerror())
      mod->dlhandle = 0;
  }
  if(mod->dlhandle)
  {
    mod->create_nodes   = dlsym(mod->dlhandle, "create_nodes");
    mod->modify_roi_out = dlsym(mod->dlhandle, "modify_roi_out");
    mod->modify_roi_in  = dlsym(mod->dlhandle, "modify_roi_in");
  }
  // TODO: go through empty callbacks once here and fill defaults:
  if(!mod->modify_roi_out) mod->modify_roi_out = &modify_roi_out;
  if(!mod->modify_roi_in ) mod->modify_roi_in  = &modify_roi_in;

  // read default params:
  // read param name, type, cnt, default value + bounds
  // allocate dynamically, this step here is not immediately perf critical
  snprintf(filename, sizeof(filename), "modules/%s/params", dirname);
  FILE *f = fopen(filename, "rb");
  int i = 0;
  if(!f)
  {
    mod->num_params = 0;
  }
  else
  {
    while(!feof(f))
    {
      fscanf(f, "%[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      mod->param[i++] = read_param_config_ascii(line);
      if(i > sizeof(mod->param)/sizeof(mod->param[0])) break;
    }
    mod->num_params = i;
    fclose(f);
  }

  // read connector info
  snprintf(filename, sizeof(filename), "modules/%s/connectors", dirname);
  f = fopen(filename, "rb");
  if(!f)
  {
    // TODO: clean up!
    return 1; // error, can't have zero connectors.
  }
  else
  {
    i = 0;
    while(!feof(f))
    {
      fscanf(f, "%[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      read_connector_ascii(mod->connector+i++, line);
      // TODO also init all the other variables, maybe inside this function
    }
    mod->num_connectors = i;
    fclose(f);
  }

  // TODO: more sanity checks?

  fprintf(stderr, "[module so load] loading %s\n", dirname);
  return 0;
}

// TODO: in some global cleanup:
static inline void
dt_module_so_unload(dt_module_so_t *mod)
{
  // decrements ref count and unloads the dso if it drops to 0
  if(mod->dlhandle)
    dlclose(mod->dlhandle);
}

int dt_pipe_global_init()
{
  memset(&dt_pipe, 0, sizeof(dt_pipe));
  // TODO: setup search directory
  struct dirent *dp;
  DIR *fd = opendir("modules");
  if (!fd)
  {
    fprintf(stderr, "[pipe global init] cannot open modules directory!\n");
    return 1;
  }
  int i = 0;
  while((dp = readdir(fd)))
    if(dp->d_type == DT_DIR) i++;
  dt_pipe.num_modules = i;
  dt_pipe.module = malloc(sizeof(dt_module_so_t)*dt_pipe.num_modules);
  i = 0;
  rewinddir(fd);
  while((dp = readdir(fd)))
  {
    if(dp->d_type == DT_DIR)
    {
      int err = dt_module_so_load(dt_pipe.module + i, dp->d_name);
      if(!err) i++;
    }
  }
  dt_pipe.num_modules = i;
  closedir(fd);
  return 0;
}

void dt_pipe_global_cleanup()
{
  for(int i=0;i<dt_pipe.num_modules;i++)
    dt_module_so_unload(dt_pipe.module + i);
  // TODO free all mem and dlclose()
  memset(&dt_pipe, 0, sizeof(dt_pipe));
}

