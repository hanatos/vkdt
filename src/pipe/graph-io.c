#include "pipe/graph-io.h"
#include "pipe/graph-history.h"
#include "modules/api.h"
#include "pipe/io.h"
#include "core/log.h"
#include <libgen.h>
#include <unistd.h>


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
    int         frame)
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
    for(int i=beg;i<end;i++)
      *(block++) = dt_read_float(line, &line);
  }
  else if(p->type == dt_token("int"))
  {
    int32_t *block = (int32_t *)data + beg;
    for(int i=beg;i<end;i++)
      *(block++) = dt_read_int(line, &line);
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
  return read_param_values_ascii(graph, line, name, inst, parm, 0, 0, -1);
}

// read only a subset of the parameters, given explicit indices for begin and end.
static inline int
read_paramsub_ascii(
    dt_graph_t *graph,
    char       *line)
{
  // read module:instance:param:beg:end:value x cnt
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  int beg = dt_read_int(line, &line);
  int end = dt_read_int(line, &line);
  return read_param_values_ascii(graph, line, name, inst, parm, beg, end, -1);
}

// helper to keyframe from config file
static inline int
read_keyframe_ascii(
    dt_graph_t *graph,
    char       *line)
{
  // read frame:module:instance:param:beg:end:value x cnt
  int frame = dt_read_int(line, &line);
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  uint32_t beg = dt_read_int(line, &line);
  uint32_t end = dt_read_int(line, &line);
  return read_param_values_ascii(graph, line, name, inst, parm, beg, end, frame);
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
  if(modid0 <= -1 || modid1 <= -1 || modid0 >= graph->num_modules || modid1 >= graph->num_modules)
  {
    dt_log(s_log_pipe, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    dt_log(s_log_pipe, "[read connect] no such modules %d %d", modid0, modid1);
    return -1;
  }
  int conid0 = dt_module_get_connector(graph->module+modid0, conn0);
  int conid1 = dt_module_get_connector(graph->module+modid1, conn1);
  int err = dt_module_connect(graph, modid0, conid0, modid1, conid1);
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
    return -err;
  }
  else
  {
    // graph->module[modid0].connector[conid0].flags |= extra_flags;
    graph->module[modid1].connector[conid1].flags |= extra_flags;
    if(extra_flags & s_conn_feedback)
    { // set this here because during dag traversal it would potentially
      // update too many connectors (connected to multiple inputs w/o feedback?)
      graph->module[modid0].connector[conid0].frames = 2;
      graph->module[modid1].connector[conid1].frames = 2;
    }
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
  // in case of failure:
  // discard module id, but remember error state (returns modid=-1)
  int modid = dt_module_add(graph, name, inst);
  if(modid < 0) return -1; // this is a fatal error
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
  else if(cmd == dt_token("paramsub")) return read_paramsub_ascii(graph, c);
  else if(cmd == dt_token("keyframe")) return read_keyframe_ascii(graph, c);
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
  snprintf(graph->basedir, sizeof(graph->basedir), "%s", dt_pipe.basedir); // take copy for modules without global access
  char target[256] = {0};
  const char *f = target;
  ssize_t err = readlink(filename, target, sizeof(target));
  if(err == -1) // mostly not a link. might be a different error but we don't want to use target.
    f = filename;

  if(snprintf(graph->searchpath, sizeof(graph->searchpath), "%s", f) < 0)
  {
    graph->searchpath[0] = 0;
    return 1;
  }
  else
  {
    // don't use: dirname(graph->searchpath) since it may or may not alter graph->searchpath, implementation dependent.
    char *c = 0;
    for(int i=0;graph->searchpath[i]!=0;i++) if(graph->searchpath[i] == '/') c = graph->searchpath+i;
    if(c) *c = 0; // get dirname, i.e. strip off executable name
    else { c = graph->searchpath; c[0] = '.'; c[1] = '/'; c[2] = 0; } // found no '/', gotta use './'
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
  FILE *f = fopen(filename, "rb");
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
  WRITE("module:%"PRItkn":%"PRItkn"\n",
      dt_token_str(graph->module[m].name),
      dt_token_str(graph->module[m].inst));
  return line;
}

// write connection
char *
dt_graph_write_connection_ascii(
    const dt_graph_t *graph,
    const int         m,      // module index
    const int         i,      // connector index on given module
    char             *line,
    size_t            size)
{
  dt_connector_t *c = graph->module[m].connector+i;
  if(!dt_connector_input(c)) return line; // refuse to serialise outgoing connections
  if(c->connected_mi == -1)  return line; // not connected
  WRITE("%s:"
      "%"PRItkn":%"PRItkn":%"PRItkn":"
      "%"PRItkn":%"PRItkn":%"PRItkn"\n",
      c->flags & s_conn_feedback ? "feedback" : "connect",
      dt_token_str(graph->module[c->connected_mi].name),
      dt_token_str(graph->module[c->connected_mi].inst),
      dt_token_str(graph->module[c->connected_mi].connector[c->connected_mc].name),
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
    char            **eop)
{
  const dt_module_t *mod = graph->module + m;
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
  WRITE("keyframe:%d:%"PRItkn":%"PRItkn":%"PRItkn":%d:%d:",
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
      if(!(buf = dt_graph_write_connection_ascii(graph, m, i, buf, end-buf)))
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

// helper to read and replace
static inline int
scanline_replace(
    FILE *f,
    char *line,
    const int num_rules,
    dt_token_t *search,
    dt_token_t *replace)
{
  char buf[300000];
  fscanf(f, "%299999[^\n]", buf);
  if(fgetc(f) == EOF) return 1; // read \n // TODO: check if this is in fact '\n'?
  int slen[num_rules];
  int dlen[num_rules];
  for(int r=0;r<num_rules;r++)
  {
    slen[r] = strnlen(dt_token_str(search [r]), 8);
    dlen[r] = strnlen(dt_token_str(replace[r]), 8);
  }
  char *esi = buf, *edi = line;
  while(*esi != '\n' && *esi != 0)
  {
    for(int r=0;r<=num_rules;r++)
    {
      if(r == num_rules)
      {
        *edi = *esi;
        edi++; esi++;
      }
      else if(!strncmp(esi, dt_token_str(search[r]), slen[r]))
      {
        memcpy(edi, dt_token_str(replace[r]), dlen[r]);
        esi += slen[r];
        edi += dlen[r];
      }
    }
  }
  *edi = 0;
  return 0;
}

int
dt_graph_read_block(
    dt_graph_t *graph,
    const char *filename,
    dt_token_t inst,
    dt_token_t out_mod,
    dt_token_t out_inst,
    dt_token_t out_conn,
    dt_token_t in_mod,
    dt_token_t in_inst,
    dt_token_t in_conn)
{
  FILE *f = dt_graph_open_resource(graph, filename, "rb");
  if(f)
  { // read lines individually, we need to search/replace generic input/output/instance strings
    // needs to be large enough to hold 1000 vertices of drawn masks:
    char line[300000];
    dt_token_t search [] = {
      dt_token("INSTANCE"),
      dt_token("OUTMOD"), dt_token("OUTINST"), dt_token("OUTCONN"),
      dt_token("INMOD"),  dt_token("ININST"),  dt_token("INCONN")};
    dt_token_t replace[] = {inst, out_mod, out_inst, out_conn, in_mod, in_inst, in_conn};
    uint32_t lno = 0;
    while(!feof(f))
    {
      if(scanline_replace(f, line, 7, search, replace)) break;
      lno++;
      // just ignore whatever goes wrong:
      if(dt_graph_read_config_line(graph, line))
        dt_log(s_log_pipe, "failed in line %u: '%s'", lno, line);
      dt_graph_history_line(graph, line);
    }
    fclose(f);
    return 0;
  }
  dt_log(s_log_pipe|s_log_err, "could not open '%s'", filename);
  return 1;
}
