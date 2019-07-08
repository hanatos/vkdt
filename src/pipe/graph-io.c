#include "pipe/graph-io.h"
#include "modules/api.h"
#include "pipe/io.h"
#include "core/log.h"

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
  switch(p->type)
  {
    case dt_token_static("float"):
    {
      float *block = (float *)(graph->module[modid].param + p->offset);
      for(int i=0;i<cnt;i++)
        *(block++) = dt_read_float(line, &line);
    }
    break;
    case dt_token_static("string"):
    {
      char *str = (char *)(graph->module[modid].param + p->offset);
      int i = 0;
      do str[i++] = *(line++);
      while(line[0] && (i < cnt-1));
      str[i] = 0;
    }
    break;
    default:
    dt_log(s_log_err|s_log_pipe, "unknown param type %"PRItkn, dt_token_str(p->type));
  }
  return 0;
}

// helper to read a connection information from config file
static inline int
read_connection_ascii(
    dt_graph_t *graph,
    char       *line)
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
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    dt_log(s_log_pipe, "[read connect] connection failed: error %d: %s", err, dt_connector_error_str(err));
    return err;
  }
  return 0;
}

// helper to add a new module from config file
static inline int
read_module_ascii(
    dt_graph_t *graph,
    char       *line)
{
  // TODO: how does this know it failed?
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  // discard module id, but remember error state (returns modid=-1)
  return dt_module_add(graph, name, inst) < 0;
}

// this is a public api function on the graph, it reads the full stack
int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;
  char line[2048];
  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    char *c = line;
    lno++;
    if(line[0] == '#') continue;
    dt_token_t cmd = dt_read_token(c, &c);
    if     (cmd == dt_token("module"))  { if(read_module_ascii(graph, c))     goto error;}
    else if(cmd == dt_token("connect")) { if(read_connection_ascii(graph, c)) goto error;}
    else if(cmd == dt_token("param"))   { if(read_param_ascii(graph, c))      goto error;}
    else goto error;
  }
  fclose(f);
  return 0;
error:
  dt_log(s_log_pipe|s_log_err, "failed in line %u: '%s'", lno, line);
  fclose(f);
  return 1;
}

int dt_graph_write_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  FILE *f = fopen(filename, "wb");
  if(!f) return 1;
  // write all modules
  for(int m=0;m<graph->num_modules;m++)
    fprintf(f, "module:%"PRItkn":%"PRItkn"\n",
        dt_token_str(graph->module[m].name),
        dt_token_str(graph->module[m].inst));
  // write all connections
  for(int m=0;m<graph->num_modules;m++)
  {
    for(int i=0;i<graph->module[m].num_connectors;i++)
    {
      dt_connector_t *c = graph->module[m].connector+i;
      if(dt_connector_input(c))
        fprintf(f, "connect:"
            "%"PRItkn":%"PRItkn":%"PRItkn":"
            "%"PRItkn":%"PRItkn":%"PRItkn"\n",
            dt_token_str(graph->module[c->connected_mi].name),
            dt_token_str(graph->module[c->connected_mi].inst),
            dt_token_str(graph->module[c->connected_mi].connector[c->connected_mc].name),
            dt_token_str(graph->module[m].name),
            dt_token_str(graph->module[m].inst),
            dt_token_str(c->name));
    }
  }
  // write all params
  for(int m=0;m<graph->num_modules;m++)
  {
    dt_module_t *mod = graph->module + m;
    for(int p=0;p<mod->so->num_params;p++)
    {
      fprintf(f, "param:%"PRItkn":%"PRItkn":%"PRItkn":",
          dt_token_str(mod->name),
          dt_token_str(mod->inst),
          dt_token_str(mod->so->param[p]->name));
      switch(mod->so->param[p]->type)
      {
      case dt_token_static("float"):
      {
        const float *v = dt_module_param_float(mod, p);
        for(int i=0;i<mod->so->param[p]->cnt-1;i++)
          fprintf(f, "%g:", v[i]);
        fprintf(f, "%g\n", v[mod->so->param[p]->cnt-1]);
        break;
      }
      case dt_token_static("string"):
      {
        fprintf(f, "%s\n", dt_module_param_string(mod, p));
        break;
      }
      default:;
      }
    }
  }
  fclose(f);
  return 0;
}
