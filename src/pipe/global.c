#include "global.h"
#include "io.h"
#include "module.h"
#include "graph.h"
#include "core/log.h"

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
  dt_log(s_log_pipe, "param %"PRItkn" %"PRItkn, dt_token_str(name), dt_token_str(type));
  int cnt = dt_read_int(line, &line);
  // TODO: sanity check and clamp?
  dt_ui_param_t *p = malloc(sizeof(*p) + cnt*3*sizeof(float));
  p->name = name;
  p->type = type;
  p->cnt = cnt;
  float *val = p->val;
  for(int i=0;i<p->cnt;i++)
  {
    *(val++) = dt_read_float(line, &line); // default
    *(val++) = dt_read_float(line, &line); // min
    *(val++) = dt_read_float(line, &line); // max
  }
  return p;
}

static inline int
dt_module_so_load(
    dt_module_so_t *mod,
    const char *dirname)
{
  if(!strcmp(dirname, ".") || !strcmp(dirname, "..")) return 1;
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
    if(!mod->dlhandle)
      dt_log(s_log_pipe|s_log_err, dlerror());
  }
  if(mod->dlhandle)
  {
    mod->create_nodes   = dlsym(mod->dlhandle, "create_nodes");
    mod->modify_roi_out = dlsym(mod->dlhandle, "modify_roi_out");
    mod->modify_roi_in  = dlsym(mod->dlhandle, "modify_roi_in");
    mod->init           = dlsym(mod->dlhandle, "init");
    mod->cleanup        = dlsym(mod->dlhandle, "cleanup");
    mod->write_sink     = dlsym(mod->dlhandle, "write_sink");
    mod->read_source    = dlsym(mod->dlhandle, "read_source");
    mod->commit_params  = dlsym(mod->dlhandle, "commit_params");
  }

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
    mod->param[0]->offset = 0;
    for(int i=1;i<mod->num_params;i++) // TODO: sizeof(param.type)* !
      mod->param[i]->offset = mod->param[i-1]->offset + sizeof(float)*mod->param[i-1]->cnt;
  }

  // read connector info
  snprintf(filename, sizeof(filename), "modules/%s/connectors", dirname);
  f = fopen(filename, "rb");
  if(!f)
  {
    // TODO: clean up!
    dt_log(s_log_pipe|s_log_err, "module %s has no connectors!", dirname);
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
      dt_log(s_log_pipe, "connector %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn,
          dt_token_str(mod->connector[i-1].name),
          dt_token_str(mod->connector[i-1].type),
          dt_token_str(mod->connector[i-1].chan),
          dt_token_str(mod->connector[i-1].format));
    }
    mod->num_connectors = i;
    fclose(f);
  }

  // TODO: more sanity checks?

  dt_log(s_log_pipe, "[module so load] loading %s", dirname);
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
    dt_log(s_log_pipe, "[global init] cannot open modules directory!");
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

int dt_module_get_param(dt_module_so_t *so, dt_token_t param)
{
  for(int i=0;i<so->num_params;i++)
    if(so->param[i]->name == param) return i;
  return -1;
}
