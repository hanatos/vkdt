#include "pipe/graph-io.h"
#include "modules/api.h"
#include "pipe/io.h"
#include "core/log.h"
#include <libgen.h>

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
  // grab count from declaration in module_so_t:
  int modid = dt_module_get(graph, name, inst);
  if(modid < 0 || modid > graph->num_modules)
  {
    dt_log(s_log_err|s_log_pipe, "no such module/instance %"PRItkn"/%"PRItkn,
        dt_token_str(name), dt_token_str(inst));
    return 1;
  }
  int parid = dt_module_get_param(graph->module[modid].so, parm);
  if(parid < 0)
  {
    dt_log(s_log_err|s_log_pipe, "no such parameter name %"PRItkn, dt_token_str(parm));
    return 2;
  }
  const dt_ui_param_t *p = graph->module[modid].so->param[parid];
  int cnt = p->cnt;
  if(p->type == dt_token("float"))
  {
    float *block = (float *)(graph->module[modid].param + p->offset);
    for(int i=0;i<cnt;i++)
      *(block++) = dt_read_float(line, &line);
  }
  else if(p->type == dt_token("int"))
  {
    int32_t *block = (int32_t *)(graph->module[modid].param + p->offset);
    for(int i=0;i<cnt;i++)
      *(block++) = dt_read_int(line, &line);
  }
  else if(p->type == dt_token("string"))
  {
    char *str = (char *)(graph->module[modid].param + p->offset);
    int i = 0;
    do str[i++] = *(line++);
    while(line[0] && (i < cnt-1));
    str[i] = 0;
  }
  else dt_log(s_log_err|s_log_pipe, "unknown param type %"PRItkn, dt_token_str(p->type));
  return 0;
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
    return 1;
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
    return err;
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
  return dt_module_add(graph, name, inst) < 0;
}

int dt_graph_read_config_line(
    dt_graph_t *graph,
    char *c)
{
  if(c[0] == '#') return 0;
  dt_token_t cmd = dt_read_token(c, &c);
  if     (cmd == dt_token("module"))   return read_module_ascii(graph, c);
  else if(cmd == dt_token("param"))    return read_param_ascii(graph, c);
  else if(cmd == dt_token("connect"))  return read_connection_ascii(graph, c, 0);
  else if(cmd == dt_token("feedback")) return read_connection_ascii(graph, c, s_conn_feedback);
  else if(cmd == dt_token("frames"))   graph->frame_cnt = atol(c); // does not fail
  else return 1;
  return 0;
}

// TODO: rewrite this to work on a uint8_t * data pointer (same for write below)
// TODO: also insert line start pointers (for history stack)
// this is a public api function on the graph, it reads the full stack
int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(snprintf(graph->searchpath, sizeof(graph->searchpath), "%s", filename) < 0) graph->searchpath[0] = 0;
  else dirname(graph->searchpath);
  if(!f) return 1;
  // needs to be large enough to hold 1000 vertices of drawn masks:
  char line[30000];
  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    lno++;
    if(dt_graph_read_config_line(graph, line)) goto error;
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
// serialises only incoming connections (others can't be resolved due to ambiguity)
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
    size_t            size)
{
  const dt_module_t *mod = graph->module + m;
  WRITE("param:%"PRItkn":%"PRItkn":%"PRItkn":",
      dt_token_str(mod->name),
      dt_token_str(mod->inst),
      dt_token_str(mod->so->param[p]->name));
  if(mod->so->param[p]->type == dt_token("float"))
  {
    const float *v = dt_module_param_float(mod, p);
    for(int i=0;i<mod->so->param[p]->cnt-1;i++)
      WRITE("%g:", v[i]);
    WRITE("%g\n", v[mod->so->param[p]->cnt-1]);
  }
  else if(mod->so->param[p]->type == dt_token("int"))
  {
    const int32_t *v = dt_module_param_int(mod, p);
    for(int i=0;i<mod->so->param[p]->cnt-1;i++)
      WRITE("%d:", v[i]);
    WRITE("%d\n", v[mod->so->param[p]->cnt-1]);
  }
  else if(mod->so->param[p]->type == dt_token("string"))
  {
    WRITE("%s\n", dt_module_param_string(mod, p));
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
  return line;
}
#undef WRITE

// use individual functions above and write whole configuration in flat:
// leave no parameters behind and discard history.
int dt_graph_write_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  size_t size = 64000;
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
      if(!(buf = dt_graph_write_param_ascii(graph, m, p, buf, end-buf)))
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
