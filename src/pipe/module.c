#include "module.h"
#include "graph.h"
#include "core/log.h"
#include "modules/api.h"

// this is a public api function
int dt_module_add(
    dt_graph_t *graph,
    dt_token_t name,
    dt_token_t inst)
{
  // add to graph's list
  // make sure we have enough memory, realloc if not
  if(graph->num_modules >= graph->max_modules-1)
  {
    assert(0 && "TODO: realloc module graph arrays");
    return -1;
  }
  const int modid = graph->num_modules++;

  dt_module_t *mod = graph->module + modid;
  mod->name = name;
  mod->inst = inst;
  mod->graph = graph;
  mod->data = 0;
  mod->committed_param_size = 0;
  mod->committed_param = 0;

  // copy over initial info from module class:
  for(int i=0;i<dt_pipe.num_modules;i++)
  { // TODO: speed this up somehow?
    if(name == dt_pipe.module[i].name)
    {
      mod->so = dt_pipe.module + i;
      // init params:
      mod->param_size = 0;
      if(mod->so->num_params)
      {
        dt_ui_param_t *p = mod->so->param[mod->so->num_params-1];
        mod->param_size = p->offset + dt_ui_param_size(p->type, p->cnt);
        mod->param = graph->params_pool + graph->params_end;
        graph->params_end += mod->param_size;
        assert(graph->params_end <= graph->params_max);
      }
      for(int p=0;p<mod->so->num_params;p++)
      { // init default params
        dt_ui_param_t *pp = mod->so->param[p];
        memcpy(mod->param + pp->offset, pp->val, dt_ui_param_size(pp->type, pp->cnt));
      }
      for(int c=0;c<mod->so->num_connectors;c++)
      { // init connectors from our module class:
        mod->connector[c] = mod->so->connector[c];
        dt_connector_t *cn = mod->connector+c;
        cn->array_length = 1; // modules don't support arrays for now
        // set connector's ref id's to -1 or ref count to 0 if a write|source node
        if(cn->type == dt_token("read") || cn->type == dt_token("sink"))
        {
          cn->connected_mi = -1;
          cn->connected_mc = -1;
        }
        else if(cn->type == dt_token("write") || cn->type == dt_token("source"))
        {
          cn->connected_mi = 0;
          cn->connected_mc = 0;
        }
      }
      mod->num_connectors = mod->so->num_connectors;
      break;
    }
  }
  if(mod->num_connectors == 0)
  { // if connectors still empty fail
    dt_log(s_log_pipe|s_log_err, "module %"PRItkn" has no connectors!", dt_token_str(name));
    graph->num_modules--;
    return -1;
  }

  if(mod->so->init)
  {
    mod->so->init(mod);
    if(mod->committed_param_size)
    {
      mod->committed_param = graph->params_pool + graph->params_end;
      graph->params_end += mod->committed_param_size;
      assert(graph->params_end <= graph->params_max);
    }
  }
  return modid;
}

int dt_module_get_connector(
    const dt_module_t *m, dt_token_t conn)
{
  assert(m->num_connectors < DT_MAX_CONNECTORS);
  for(int c=0;c<m->num_connectors;c++)
    if(m->connector[c].name == conn) return c;
  return -1;
}

int dt_module_remove(
    dt_graph_t *graph,
    const int modid)
{
  // TODO: mark as dead and increment dead counter. if we don't like it any
  // more, perform some form of garbage collection.
  assert(modid >= 0 && graph->num_modules > modid);
  if(graph->module[modid].so->cleanup)
    graph->module[modid].so->cleanup(graph->module+modid);
  // disconnect all channels
  for(int c=0;c<graph->module[modid].num_connectors;c++)
    if(dt_module_connect(graph, -1, -1, modid, c))
      return -1;
  graph->module[modid].name = 0;
  graph->module[modid].inst = 0;
  graph->module[modid].connector[0].type = 0; // to avoid being detected as sink
  return 0;
}

// return modid or -1
int dt_module_get(
    const dt_graph_t *graph,
    dt_token_t name,
    dt_token_t inst)
{
  // TODO: some smarter way? hash table? benchmark how much we lose
  // through this stupid O(n) approach. after all the comparisons are
  // fast and we don't have all that many modules.
  for(int i=0;i<graph->num_modules;i++)
    if(graph->module[i].name == name &&
       graph->module[i].inst == inst)
      return i;
  return -1;
}

// returns count of connected modules
int dt_module_get_module_after(
    const dt_graph_t  *graph,
    const dt_module_t *us,
    int               *m_out,
    int               *c_out,
    int                max_cnt)
{
  int cnt = 0;
  int conn = dt_module_get_connector(us, dt_token("output"));
  int modi = us - graph->module;
  for(int m=0;m<graph->num_modules;m++)
  {
    for(int c=0;c<graph->module[m].num_connectors;c++)
    {
      if(dt_connector_input(graph->module[m].connector+c) &&
         graph->module[m].connector[c].connected_mi == modi &&
         graph->module[m].connector[c].connected_mc == conn)
      {
        m_out[cnt] = m;
        c_out[cnt] = c;
        if(++cnt >= max_cnt) return cnt;
      }
    }
  }
  return cnt;
}

int dt_module_get_module_before(const dt_graph_t *graph, const dt_module_t *us, int *cout)
{
  int conn = dt_module_get_connector(us, dt_token("input"));
  if(conn == -1) return -1;
  if(cout) *cout = us->connector[conn].connected_mc;
  return us->connector[conn].connected_mi;
}
