#include "global.h"
#include "asciiio.h"
#include "module.h"
#include "graph.h"
#include "res.h"
#include "core/log.h"
#include "core/fs.h"
#include "modules/api.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <locale.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

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
  // tkn:tkn:int:float
  // name:type:cnt:defval
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t type = dt_read_token(line, &line);
  // dt_log(s_log_pipe, "param %"PRItkn" %"PRItkn, dt_token_str(name), dt_token_str(type));
  int cnt = dt_read_int(line, &line);
  // TODO: sanity check and clamp?
  dt_ui_param_t *p = malloc(sizeof(*p) + dt_ui_param_size(type, cnt));
  p->name = name;
  p->type = type;
  p->cnt = cnt;
  if(p->cnt < 1)
    dt_log(s_log_err|s_log_pipe, "param %"PRItkn" %"PRItkn" has zero count!", dt_token_str(name), dt_token_str(type));
  if(type == dt_token("float"))
  {
    float *val = p->val;
    for(int i=0;i<p->cnt;i++)
      *(val++) = dt_read_float(line, &line); // default value
  }
  else if(type == dt_token("int"))
  {
    int32_t *val = p->vali;
    for(int i=0;i<p->cnt;i++)
      *(val++) = dt_read_int(line, &line); // default value
  }
  else if(type == dt_token("string"))
  {
    char *str = p->str;
    int i = 0;
    do str[i++] = *(line++);
    while(line[0] && (i < cnt-1));
    str[i] = 0;
  }
  else
  {
    dt_log(s_log_err|s_log_pipe, "unknown parameter type: %"PRItkn, dt_token_str(type));
  }
  return p;
}

static inline int
dt_module_so_load(
    dt_module_so_t *mod,
    const char *dirname)
{
  if(!strcmp(dirname, ".") || !strcmp(dirname, "..") || !strcmp(dirname, "shared"))
    return 1;
  memset(mod, 0, sizeof(*mod));
  mod->name = dt_token(dirname);
  char filename[PATH_MAX], line[8192];
  snprintf(filename, sizeof(filename), "%s/modules/%s/lib%s.so", dt_pipe.basedir, dirname, dirname);
  mod->dlhandle = 0;
  if(fs_isreg_file(filename))
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
    mod->bs_init        = dlsym(mod->dlhandle, "bs_init");
    mod->write_sink     = dlsym(mod->dlhandle, "write_sink");
    mod->read_source    = dlsym(mod->dlhandle, "read_source");
    mod->animate        = dlsym(mod->dlhandle, "animate");
    mod->commit_params  = dlsym(mod->dlhandle, "commit_params");
    mod->ui_callback    = dlsym(mod->dlhandle, "ui_callback");
    mod->check_params   = dlsym(mod->dlhandle, "check_params");
    mod->audio          = dlsym(mod->dlhandle, "audio");
    mod->input          = dlsym(mod->dlhandle, "input");
  }

  // init callback handles on dso side for windows:
  if(mod->bs_init) mod->bs_init();

  // read default params:
  // read param name, type, cnt, default value + bounds
  // allocate dynamically, this step here is not immediately perf critical

  snprintf(filename, sizeof(filename), "modules/%s/params", dirname);
  FILE *f = dt_graph_open_resource(0, 0, filename, "rb");
  int i = 0;
  if(!f)
  {
    mod->num_params = 0;
  }
  else
  {
    while(!feof(f))
    {
      fscanf(f, "%8191[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      mod->param[i++] = read_param_config_ascii(line);
      if(i > sizeof(mod->param)/sizeof(mod->param[0])) break;
    }
    mod->num_params = i;
    fclose(f);
    mod->param[0]->offset = 0;
    for(int i=1;i<mod->num_params;i++)
      mod->param[i]->offset = mod->param[i-1]->offset +
        dt_ui_param_type_size(mod->param[i-1]->type)*mod->param[i-1]->cnt;
  }

  // read ui widget connection:
  snprintf(filename, sizeof(filename), "modules/%s/params.ui", dirname);
  f = dt_graph_open_resource(0, 0, filename, "rb");
  // init as [0,1] sliders as fallback
  for(int i=0;i<mod->num_params;i++)
    mod->param[i]->widget = (dt_widget_descriptor_t) {
      .type = dt_token("slider"),
      .min  = 0.0f,
      .max  = 1.0f,
      .grpid = -1,
      .mode  = 0,
      .cntid = -1,
    };
  if(f)
  {
    // these files consist of lines such as:
    //  tkn:tkn:[float:float:]tkn
    //  param-name:widget-type:[min:max:]count
    // for every widget. the count specifies the param name of the number of
    // elements this widget should replicate (like white balance might have 4
    // channels, but a drawn mask might have N elements where N is specified by
    // another parameter).
    // additionally, widgets can be grouped. this is achieved by special lines:
    //  group:tkn:int
    //  group:param-name:mode
    // where param-name refers to an integer parameter which sets the mode.
    // if mode >= 100, it hides the following params for param-name=mode-100
    // if tkn==-1, the group resets to all visible.
    // a line "---" can be used before a widget definition to insert a separator space
    // before this widget in the ui.
    int grpid = -1;
    int mode = 0;
    int have_sep = 0;
    while(!feof(f))
    {
      fscanf(f, "%8191[^\n]", line);
      char *b = line;
      if(fgetc(f) == EOF) break; // read \n
      if(line[0] == '#') continue; // ignore comment lines
      dt_token_t parm = dt_read_token(b, &b);
      dt_token_t type = dt_read_token(b, &b);
      if(parm == dt_token("---"))
      { // next line's ui definition has a pre-separator
        have_sep = 1;
        continue;
      }
      float min = 0.0f, max = 0.0f;
      void *data = 0;
      if(parm == dt_token("group"))
      {
        grpid = dt_module_get_param(mod, type);
        mode = dt_read_int(b, &b);
        continue;
      }
      else if(type == dt_token("slider")
           || type == dt_token("vslider")
           || type == dt_token("rgb"))
      { // read range of slider
        min = dt_read_float(b, &b);
        max = dt_read_float(b, &b);
      }
      else if(type == dt_token("combo") || type == dt_token("bitmask") || type == dt_token("btngrid"))
      { // read list of names of all entries
        size_t len = 0;
        for(len=0;len<sizeof(line) - (b-line);len++)
        { // we replace all ':' by 0 terminated strings.
          // this means combo boxes don't support multi-count widgets.
          if(b[len] == 0) break;
          if(b[len] == ':') b[len] = 0;
        }
        data = malloc(len+2);
        memcpy(data, b, len);
        ((char*)data)[len] = ((char*)data)[len+1] = 0;
        b += len; // set pointer to the end
      }
      else if(type == dt_token("ab"))      {}
      else if(type == dt_token("colour"))  {}
      else if(type == dt_token("straight")){}
      else if(type == dt_token("crop"))    {}
      else if(type == dt_token("draw"))    {}
      else if(type == dt_token("filename")){}
      else if(type == dt_token("grab"))    {}
      else if(type == dt_token("hidden"))  {}
      else if(type == dt_token("pers"))    {}
      else if(type == dt_token("pick"))    {}
      else if(type == dt_token("print"))   {}
      else if(type == dt_token("rbmap"))   {}
      else if(type == dt_token("callback")){}
      else if(type == dt_token("coledit")) {}
      else if(type == dt_token("rgbknobs")){}
      else dt_log(s_log_err, "unknown widget type %"PRItkn" in %s!", dt_token_str(type), filename);
      int pid = dt_module_get_param(mod, parm);
      if(pid == -1)
      {
        dt_log(s_log_pipe, "unknown param `%"PRItkn"' in %s!", dt_token_str(parm), filename);
        continue;
      }
      int cntid = -1;
      if(b[-1] == ':')
      {
        dt_token_t count = dt_read_token(b, &b);
        cntid = dt_module_get_param(mod, count);
        if(cntid == -1)
          dt_log(s_log_err, "unknown count ref %"PRItkn" for param %"PRItkn" in %s!",
              dt_token_str(count), dt_token_str(parm), filename);
      }
      mod->param[pid]->widget = (dt_widget_descriptor_t) {
        .type  = type,
        .min   = min,
        .max   = max,
        .grpid = grpid,
        .mode  = mode,
        .cntid = cntid,
        .sep   = have_sep,
        .data  = data,
      };
      have_sep = 0;
    }
    fclose(f);
  }

  // read extracted parameter tooltips
  snprintf(filename, sizeof(filename), "modules/%s/ptooltips", dirname);
  f = dt_graph_open_resource(0, 0, filename, "rb");
  for(int i=0;i<mod->num_params;i++) mod->param[i]->tooltip = 0; // de-init
  if(f)
  {
    while(!feof(f))
    {
      char *b = line;
      fscanf(f, "%8191[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      dt_token_t pn = dt_read_token(b, &b);
      for(int i=0;i<mod->num_params;i++)
      {
        if(mod->param[i]->name == pn)
        {
          size_t len = strnlen(b, sizeof(line)-9); // remove max token + :
          char *copy = malloc(sizeof(char) * (len+1));
          strncpy(copy, b, len);
          copy[len] = 0; // make sure it's terminated even upon truncation
          mod->param[i]->tooltip = copy;
          break;
        }
      }
    }
    fclose(f);
  }
  // read connector info
  snprintf(filename, sizeof(filename), "modules/%s/connectors", dirname);
  f = dt_graph_open_resource(0, 0, filename, "rb");
  if(!f)
  {
    dt_log(s_log_pipe, "module %s has no connectors!", dirname);
    return 1; // error, can't have zero connectors.
  }
  else
  {
    i = 0;
    while(!feof(f))
    {
      fscanf(f, "%8191[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      read_connector_ascii(mod->connector+i++, line);
      // TODO also init all the other variables, maybe inside this function
      // dt_log(s_log_pipe, "connector %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn,
      //     dt_token_str(mod->connector[i-1].name),
      //     dt_token_str(mod->connector[i-1].type),
      //     dt_token_str(mod->connector[i-1].chan),
      //     dt_token_str(mod->connector[i-1].format));
    }
    mod->num_connectors = i;
    fclose(f);
  }

  // read extracted connector tooltips
  snprintf(filename, sizeof(filename), "modules/%s/ctooltips", dirname);
  f = dt_graph_open_resource(0, 0, filename, "rb");
  for(int i=0;i<mod->num_connectors;i++) mod->connector[i].tooltip = 0; // de-init
  if(f)
  {
    while(!feof(f))
    {
      char *b = line;
      fscanf(f, "%8191[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      dt_token_t cn = dt_read_token(b, &b);
      for(int i=0;i<mod->num_connectors;i++)
      {
        if(mod->connector[i].name == cn)
        {
          size_t len = strnlen(b, sizeof(line)-9); // remove max token + :
          char *copy = malloc(sizeof(char) * (len+1));
          strncpy(copy, b, len);
          copy[len] = 0; // make sure it's terminated even upon truncation
          mod->connector[i].tooltip = copy;
          break;
        }
      }
    }
    fclose(f);
  }

  // find out whether this defines a simple module
  int found_input = 0, found_output = 0, num_outputs = 0;
  dt_token_t fmt = dt_token("*"), chn = dt_token("*");
  for(int i=0;i<mod->num_connectors;i++)
  { // TODO: also test for &input style definitions?
#define CHECK(X,Y) (X != dt_token("*") && mod->connector[i].Y != dt_token("*") && mod->connector[i].Y != X)
    if(dt_connector_output(mod->connector+i))
    {
      if(mod->connector[i].name != dt_token("dspy")) num_outputs++; // dspy is just a temporary thing and doesn't block us
      if(mod->connector[i].name == dt_token("output"))
      {
        found_output = !(CHECK(fmt,format) || CHECK(chn,chan));
        fmt = mod->connector[i].format;
        chn = mod->connector[i].chan;
      }
    }
    if(dt_connector_input(mod->connector+i) && mod->connector[i].name == dt_token("input"))
    {
      found_input = !(CHECK(fmt,format) || CHECK(chn,chan));
      fmt = mod->connector[i].format;
      chn = mod->connector[i].chan;
    }
#undef CHECK
  }
  mod->has_inout_chain = found_input==1 && found_output==1 && num_outputs==1;

  // TODO: more sanity checks?

  // dt_log(s_log_pipe, "[module so load] loading %s", dirname);
  return 0;
}

static inline void
dt_module_so_unload(dt_module_so_t *mod)
{
  for(int i=0;i<mod->num_params;i++)
  {
    free(mod->param[i]->widget.data);
    free((char *)mod->param[i]->tooltip);
    free(mod->param[i]);
  }
  for(int i=0;i<mod->num_connectors;i++)
    free((char *)mod->connector[i].tooltip);
  // decrements ref count and unloads the dso if it drops to 0
  if(mod->dlhandle)
    dlclose(mod->dlhandle);
}

static inline int
compare_module_name(const void *a, const void *b)
{
  const dt_module_so_t *ma = a;
  const dt_module_so_t *mb = b;
  return strncmp(dt_token_str(ma->name), dt_token_str(mb->name), 8);
}

int dt_pipe_global_init()
{
  memset(&dt_pipe, 0, sizeof(dt_pipe));
#ifdef __ANDROID__
  dt_pipe.app = appv;
#endif
  (void)setlocale(LC_ALL, "C"); // make sure we write and parse floats correctly
  // setup search directory
  fs_basedir(dt_pipe.basedir, sizeof(dt_pipe.basedir));
#ifndef __ANDROID__
  fs_homedir(dt_pipe.homedir, sizeof(dt_pipe.homedir));
#else
  snprintf(dt_pipe.homedir, sizeof(dt_pipe.homedir), "%s", dt_pipe.app->activity->internalDataPath);
#endif
  dt_log(s_log_pipe, "base directory %s", dt_pipe.basedir);
  dt_log(s_log_pipe, "home directory %s", dt_pipe.homedir);
  void *fd = dt_res_opendir("modules", 1);
  if (!fd)
  {
    dt_log(s_log_pipe, "[global init] cannot open modules directory!");
    return 1;
  }
  int i = 0;
  const char *basename = 0;
  while((basename = dt_res_next_basename(fd, 1))) i++;
  dt_pipe.num_modules = i;
  dt_pipe.module = malloc(sizeof(dt_module_so_t)*dt_pipe.num_modules);
  i = 0;
  dt_res_rewinddir(fd, 1);
  while((basename = dt_res_next_basename(fd, 1)))
  {
    int err = dt_module_so_load(dt_pipe.module + i, basename);
    if(!err) i++;
  }
  dt_pipe.num_modules = i;
  dt_log(s_log_pipe, "loaded %d modules", dt_pipe.num_modules);
  dt_res_closedir(fd, 1);
  // now sort modules alphabetically for convenience in gui later:
  qsort(dt_pipe.module, dt_pipe.num_modules, sizeof(dt_pipe.module[0]), &compare_module_name);
  return 0;
}

void dt_pipe_global_cleanup()
{
  for(int i=0;i<dt_pipe.num_modules;i++)
    dt_module_so_unload(dt_pipe.module + i);
  free(dt_pipe.module);
  memset(&dt_pipe, 0, sizeof(dt_pipe));
}
