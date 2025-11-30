#include "pipe/anim.h"
#include "pipe/graph-io.h"
#include "pipe/graph-history.h"
#include "modules/api.h"
#include "pipe/asciiio.h"
#include "core/log.h"
#include "core/fs.h"
#include <libgen.h>
#include <unistd.h>

typedef enum dt_param_write_mode_t
{
  s_param_set = 0,
  s_param_inc = 1,
  s_param_dec = 2,
}
dt_param_write_mode_t;

// helper to the helpers reading parameters in full, subsets, or for keyframes.
static inline int
read_param_values_ascii(
    dt_graph_t *graph,
    char       *line,
    dt_token_t  name,
    dt_token_t  inst,
    dt_token_t  parm,
    int         beg,
    int         end,
    int         frame,
    dt_param_write_mode_t mode,
    dt_anim_mode_t anim)
{
  int modid = dt_module_get(graph, name, inst);
  if(modid < 0 || modid > graph->num_modules)
  {
    dt_log(s_log_err|s_log_pipe, "no such module/instance %"PRItkn"/%"PRItkn,
        dt_token_str(name), dt_token_str(inst));
    return 1;
  }
  int parid = dt_module_get_param(graph->module[modid].so, parm);
  if(parid < 0)
  { // is a bit copious:
    // dt_log(s_log_err|s_log_pipe, "no such parameter name %"PRItkn, dt_token_str(parm));
    return 2;
  }
  const dt_ui_param_t *p = graph->module[modid].so->param[parid];
  int cnt = p->cnt;
  uint8_t *data = graph->module[modid].param + p->offset;
  if(beg < 0 || beg >= cnt || end < 0 || end > cnt)
  {
    dt_log(s_log_err|s_log_pipe, "parameter bounds exceeded %"PRItkn" %d,%d > %d", dt_token_str(parm), beg, end, cnt);
    return 4;
  }
  if(end == 0) end = cnt;
  if(frame >= 0)
  {
    int ki = -1;
    for(int i=0;ki<0&&i<graph->module[modid].keyframe_cnt;i++)
      if(graph->module[modid].keyframe[i].param == parm && 
         graph->module[modid].keyframe[i].frame == frame)
        ki = i;
    if(ki < 0)
    {
      ki = graph->module[modid].keyframe_cnt++;
      graph->module[modid].keyframe = dt_realloc(graph->module[modid].keyframe, &graph->module[modid].keyframe_size, sizeof(dt_keyframe_t)*(ki+1));
    }
    graph->module[modid].keyframe[ki].frame = frame;
    graph->module[modid].keyframe[ki].anim  = anim;
    graph->module[modid].keyframe[ki].param = parm;
    graph->module[modid].keyframe[ki].beg   = beg;
    graph->module[modid].keyframe[ki].end   = end;
    graph->module[modid].keyframe[ki].data  = graph->params_pool + graph->params_end;
    graph->params_end += dt_ui_param_size(p->type, p->cnt);
    assert(graph->params_end <= graph->params_max);
    data = graph->module[modid].keyframe[ki].data;
  }
  if(p->type == dt_token("float"))
  {
    float *block = (float *)data + beg;
    if(mode == s_param_set)
      for(int i=beg;i<end;i++) *(block++) = dt_read_float(line, &line);
    else if(mode == s_param_inc)
      for(int i=beg;i<end;i++) *(block++) += dt_read_float(line, &line);
    else if(mode == s_param_dec)
      for(int i=beg;i<end;i++) *(block++) -= dt_read_float(line, &line);
  }
  else if(p->type == dt_token("int"))
  {
    int32_t *block = (int32_t *)data + beg;
    if(mode == s_param_set)
      for(int i=beg;i<end;i++) *(block++) = dt_read_int(line, &line);
    else if(mode == s_param_inc)
      for(int i=beg;i<end;i++) *(block++) += dt_read_int(line, &line);
    else if(mode == s_param_dec)
      for(int i=beg;i<end;i++) *(block++) -= dt_read_int(line, &line);
  }
  else if(p->type == dt_token("string"))
  {
    char *str = (char *)data;
    int i = beg;
    do str[i++] = *(line++);
    while(line[0] && (i < end-1));
    str[i] = 0;
  }
  else dt_log(s_log_err|s_log_pipe, "unknown param type %"PRItkn, dt_token_str(p->type));
  return 0;
}

// helper to read parameters from config file
static inline int
read_param_ascii(
    dt_graph_t *graph,
    char       *line)
{
  // read module:instance:param:value x cnt
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  return read_param_values_ascii(graph, line, name, inst, parm, 0, 0, -1, s_param_set, 0);
}

// read only a subset of the parameters, given explicit indices for begin and end.
static inline int
read_paramsub_ascii(
    dt_graph_t           *graph,
    char                 *line,
    dt_param_write_mode_t mode)
{
  // read module:instance:param:beg:end:value x cnt
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  int beg = dt_read_int(line, &line);
  int end = dt_read_int(line, &line);
  return read_param_values_ascii(graph, line, name, inst, parm, beg, end, -1, mode, 0);
}

// helper to keyframe from config file
static inline int
read_keyframe_ascii(
    dt_graph_t    *graph,
    char          *line,
    dt_anim_mode_t anim)
{
  // read frame:module:instance:param:beg:end:value x cnt
  int frame = dt_read_int(line, &line);
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  uint32_t beg = dt_read_int(line, &line);
  uint32_t end = dt_read_int(line, &line);
  return read_param_values_ascii(graph, line, name, inst, parm, beg, end, frame, s_param_set, anim);
}

// helper to read a connection information from config file
static inline int
read_connection_ascii(
    dt_graph_t *graph,
    char       *line,
    int         extra_flags)
{
  dt_token_t mod0  = dt_read_token(line, &line);
  dt_token_t inst0 = dt_read_token(line, &line);
  dt_token_t conn0 = dt_read_token(line, &line);
  dt_token_t mod1  = dt_read_token(line, &line);
  dt_token_t inst1 = dt_read_token(line, &line);
  dt_token_t conn1 = dt_read_token(line, &line);

  int modid0 = dt_module_get(graph, mod0, inst0);
  int modid1 = dt_module_get(graph, mod1, inst1);
  if((mod0 != dt_token("-1") && modid0 <= -1) || modid1 <= -1 || modid0 >= (int)graph->num_modules || modid1 >= (int)graph->num_modules)
  {
    dt_log(s_log_pipe, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    dt_log(s_log_pipe, "[read connect] no such modules %d %d", modid0, modid1);
    return 1;
  }
  int conid0 = -1;
  if(mod0 == dt_token("-1")) modid0 = -1;
  else conid0 = dt_module_get_connector(graph->module+modid0, conn0);
  int  conid1 = dt_module_get_connector(graph->module+modid1, conn1);
  int err = extra_flags & s_conn_feedback ?
    dt_module_feedback(graph, modid0, conid0, modid1, conid1) :
    dt_module_connect (graph, modid0, conid0, modid1, conid1);
  if(err)
  {
    dt_log(s_log_pipe, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn,
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    dt_log(s_log_pipe, "[read connect] connection failed: error %d: %s", err, dt_connector_error_str(err));
    dt_log(s_log_pipe, "[read connect] %"PRItkn":%"PRItkn" -> %"PRItkn":%"PRItkn,
        dt_token_str(graph->module[modid0].connector[conid0].chan),
        dt_token_str(graph->module[modid0].connector[conid0].format),
        dt_token_str(graph->module[modid1].connector[conid1].chan),
        dt_token_str(graph->module[modid1].connector[conid1].format));
    return err;
  }
  else
  {
    graph->module[modid1].connector[conid1].flags |= extra_flags;
  }
  return 0;
}

// helper to add a new module from config file
static inline int
read_module_ascii(
    dt_graph_t *graph,
    char       *line)
{
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  float x = dt_read_float(line, &line);
  float y = dt_read_float(line, &line);
  // in case of failure:
  // discard module id, but remember error state (returns modid=-1)
  int modid = dt_module_add(graph, name, inst);
  if(modid < 0)
  { // okay this is bad but what can you do. life goes on, maybe the user will fix it:
    dt_log(s_log_pipe|s_log_err, "failed to add module %"PRItkn" %"PRItkn,
        dt_token_str(name), dt_token_str(inst));
    return 1;
  }
  graph->module[modid].gui_x = x;
  graph->module[modid].gui_y = y;
  return 0;
}

int dt_graph_read_config_line(
    dt_graph_t *graph,
    char *c)
{
  if(c[0] == '#') return 0;
  dt_token_t cmd = dt_read_token(c, &c);
  if     (cmd == dt_token("module"))   return read_module_ascii(graph, c);
  else if(cmd == dt_token("param"))    return read_param_ascii(graph, c);
  else if(cmd == dt_token("paramsub")) return read_paramsub_ascii(graph, c, s_param_set);
  else if(cmd == dt_token("paraminc")) return read_paramsub_ascii(graph, c, s_param_inc);
  else if(cmd == dt_token("paramdec")) return read_paramsub_ascii(graph, c, s_param_dec);
  else if(cmd == dt_token("keyframe")) return read_keyframe_ascii(graph, c, s_anim_lerp);
  else if(cmd == dt_token("keyframE")) return read_keyframe_ascii(graph, c, s_anim_ease_out);
  else if(cmd == dt_token("Keyframe")) return read_keyframe_ascii(graph, c, s_anim_ease_in);
  else if(cmd == dt_token("KeyframE")) return read_keyframe_ascii(graph, c, s_anim_smooth);
  else if(cmd == dt_token("keyFRAME")) return read_keyframe_ascii(graph, c, s_anim_step);
  else if(cmd == dt_token("connect"))  return read_connection_ascii(graph, c, 0);
  else if(cmd == dt_token("feedback")) return read_connection_ascii(graph, c, s_conn_feedback);
  else if(cmd == dt_token("frames"))   graph->frame_cnt  = atol(c); // does not fail
  else if(cmd == dt_token("fps"))      graph->frame_rate = atof(c); // does not fail
  else return 1;
  return 0;
}

int dt_graph_set_searchpath(
    dt_graph_t *graph,
    const char *filename)
{
  char target[PATH_MAX] = {0};
  const char *f = target;
  if(!fs_realpath(filename, target)) f = filename;

  if(snprintf(graph->searchpath, sizeof(graph->searchpath), "%s", f) < 0)
  {
    graph->searchpath[0] = 0;
    return 1;
  }
  else
  {
    if(!fs_dirname(graph->searchpath))
    { // found no '/', gotta use './'
      char *c = graph->searchpath; c[0] = '.'; c[1] = '/'; c[2] = 0;
    }
    return 0;
  }
}

// TODO: rewrite this to work on a uint8_t * data pointer (same for write below)
// TODO: also insert line start pointers (for history stack)
// this is a public api function on the graph, it reads the full stack
int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  FILE *f = dt_graph_open_resource(graph, 0, filename, "rb");
  if(!f) return 1;
  dt_graph_set_searchpath(graph, filename);
  // needs to be large enough to hold 10000 vertices of drawn masks:
  char line[300000];
  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%299999[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    lno++;
    // > 0 are warnings, < 0 are fatal, 0 is success
    if(dt_graph_read_config_line(graph, line) < 0) goto error;
  }
  for(int m=0;m<graph->num_modules;m++) dt_module_keyframe_post_update(graph->module+m);
  fclose(f);
  return 0;
error:
  dt_log(s_log_pipe|s_log_err, "failed in line %u: '%s'", lno, line);
  fclose(f);
  return 1;
}

#define WRITE(...) {\
  int ret = snprintf(line, size, __VA_ARGS__); \
  if(ret >= size) return 0; \
  size -= ret; \
  line += ret; \
}

// write module to ascii encoding
char *
dt_graph_write_module_ascii(
    const dt_graph_t *graph,
    const int         m,
    char             *line,
    size_t            size)
{
  if(graph->module[m].name == 0) return line; // don't write removed modules
  WRITE("module:%"PRItkn":%"PRItkn":%g:%g\n",
      dt_token_str(graph->module[m].name),
      dt_token_str(graph->module[m].inst),
      graph->module[m].gui_x, graph->module[m].gui_y);
  return line;
}

// write connection
char *
dt_graph_write_connection_ascii(
    const dt_graph_t *graph,
    const int         m,      // module index
    const int         i,      // connector index on given module
    char             *line,
    size_t            size,
    int               allow_empty)
{
  if(graph->module[m].name == 0) return line;
  dt_connector_t *c = graph->module[m].connector+i;
  if(!dt_connector_input(c)) return line; // refuse to serialise outgoing connections
  dt_token_t name, inst, conn;
  if(c->connected_mi == -1)
  { // explicitly record disconnect event (important for history)
    if(!allow_empty) return line; // don't write disconnect events explicitly
    name = inst = conn = dt_token("-1");
  }
  else
  {
    name = graph->module[c->connected_mi].name;
    inst = graph->module[c->connected_mi].inst;
    conn = graph->module[c->connected_mi].connector[c->connected_mc].name;
  }
  WRITE("%s:"
      "%"PRItkn":%"PRItkn":%"PRItkn":"
      "%"PRItkn":%"PRItkn":%"PRItkn"\n",
      c->flags & s_conn_feedback ? "feedback" : "connect",
      dt_token_str(name),
      dt_token_str(inst),
      dt_token_str(conn),
      dt_token_str(graph->module[m].name),
      dt_token_str(graph->module[m].inst),
      dt_token_str(c->name));
  return line;
}

// write param
char *
dt_graph_write_param_ascii(
    const dt_graph_t *graph,
    const int         m,
    const int         p,
    char             *line,
    size_t            size,
    char            **eop)  // end of prefix, identifying module, instance, param
{
  const dt_module_t *mod = graph->module + m;
  if(mod->name == 0) return line; // don't write params for deleted module
  WRITE("param:%"PRItkn":%"PRItkn":%"PRItkn":",
      dt_token_str(mod->name),
      dt_token_str(mod->inst),
      dt_token_str(mod->so->param[p]->name));
  if(eop) *eop = line; // pass on end of prefix to the outside
  int cnt = mod->so->param[p]->cnt;
  if(mod->so->param[p]->name == dt_token("draw"))
  { // draw issues a lot of numbers, only output the needed ones:
    const int32_t *v = dt_module_param_int(mod, p);
    cnt = 2*v[0]+1; // vertex count + list of 2d vertices
  }
  if(mod->so->param[p]->type == dt_token("float"))
  {
    const float *v = dt_module_param_float(mod, p);
    for(int i=0;i<cnt-1;i++)
      WRITE("%g:", v[i]);
    WRITE("%g\n", v[cnt-1]);
  }
  else if(mod->so->param[p]->type == dt_token("int"))
  {
    const int32_t *v = dt_module_param_int(mod, p);
    for(int i=0;i<cnt-1;i++)
      WRITE("%d:", v[i]);
    WRITE("%d\n", v[cnt-1]);
  }
  else if(mod->so->param[p]->type == dt_token("string"))
  {
    WRITE("%s\n", dt_module_param_string(mod, p));
  }
  return line;
}

// write keyframe
char *
dt_graph_write_keyframe_ascii(
    const dt_graph_t *graph,
    const int         m,
    const int         k,
    char             *line,
    size_t            size)
{
  const dt_module_t *mod = graph->module + m;
  if(mod->name == 0) return line;
  WRITE("%s:%d:%"PRItkn":%"PRItkn":%"PRItkn":%d:%d:",
      mod->keyframe[k].anim == s_anim_step     ? "keyFRAME" :
      mod->keyframe[k].anim == s_anim_ease_in  ? "Keyframe" :
      mod->keyframe[k].anim == s_anim_ease_out ? "keyframE" :
      mod->keyframe[k].anim == s_anim_smooth   ? "KeyframE" :
      "keyframe",
      mod->keyframe[k].frame,
      dt_token_str(mod->name),
      dt_token_str(mod->inst),
      dt_token_str(mod->keyframe[k].param),
      mod->keyframe[k].beg,
      mod->keyframe[k].end);
  int beg = mod->keyframe[k].beg;
  int end = mod->keyframe[k].end;
  int p = dt_module_get_param(graph->module[m].so, mod->keyframe[k].param);
  if(mod->so->param[p]->type == dt_token("float"))
  {
    const float *v = (float *)mod->keyframe[k].data;
    for(int i=beg;i<end-1;i++)
      WRITE("%g:", v[i-beg]);
    WRITE("%g\n", v[end-beg-1]);
  }
  else if(mod->so->param[p]->type == dt_token("int"))
  {
    const int32_t *v =  (int32_t *)mod->keyframe[k].data;
    for(int i=beg;i<end-1;i++)
      WRITE("%d:", v[i-beg]);
    WRITE("%d\n", v[end-beg-1]);
  }
  else if(mod->so->param[p]->type == dt_token("string"))
  {
    WRITE("%s\n", (char *)mod->keyframe[k].data);
  }
  return line;
}


char *
dt_graph_write_global_ascii(
    const dt_graph_t *graph,
    char             *line,
    size_t            size)
{
  WRITE("frames:%d\n", graph->frame_cnt);
  WRITE("fps:%g\n",    graph->frame_rate);
  return line;
}
#undef WRITE

// use individual functions above and write whole configuration in flat:
// leave no parameters behind and discard history.
int dt_graph_write_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  size_t size = 4<<20;
  char *org = malloc(size);
  char *buf = org;
  char *end = buf + size;

  if(!(buf = dt_graph_write_global_ascii(graph, buf, end-buf)))
    goto error;

  // write all modules
  for(int m=0;m<graph->num_modules;m++)
    if(!(buf = dt_graph_write_module_ascii(graph, m, buf, end-buf)))
      goto error;

  // write all connections
  for(int m=0;m<graph->num_modules;m++)
    for(int i=0;i<graph->module[m].num_connectors;i++)
      if(!(buf = dt_graph_write_connection_ascii(graph, m, i, buf, end-buf, 0)))
        goto error;

  // write all params
  for(int m=0;m<graph->num_modules;m++)
    for(int p=0;p<graph->module[m].so->num_params;p++)
      if(!(buf = dt_graph_write_param_ascii(graph, m, p, buf, end-buf, 0)))
        goto error;

  // write all keyframes
  for(int m=0;m<graph->num_modules;m++)
    for(int k=0;k<graph->module[m].keyframe_cnt;k++)
      if(!(buf = dt_graph_write_keyframe_ascii(graph, m, k, buf, end-buf)))
        goto error;

  FILE *f = fopen(filename, "wb");
  if(!f) goto error;
  fwrite(org, 1, buf - org, f);
  fclose(f);
  free(org);
  return 0;
error:
  dt_log(s_log_err, "failed to write config file %s", filename);
  free(org);
  return 1;
}
