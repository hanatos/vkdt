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
  int modid = -1;
  for(int i=0;i<graph->num_modules;i++)
  { // dedup if existing:
    if(graph->module[i].name == name && graph->module[i].inst == inst)
      return i;
    if(graph->module[i].name == 0)
    { // recycle empty/previously deleted modules
      modid = i;
      break;
    }
  }

  // add to graph's list
  if(modid == -1)
  { // make sure we have enough memory, realloc if not
    if(graph->num_modules >= graph->max_modules-1)
    {
      assert(0 && "TODO: realloc module graph arrays");
      return -1;
    }
    modid = graph->num_modules++;
  }

  dt_module_t *mod = graph->module + modid;
  mod->name = name;
  mod->inst = inst;
  mod->graph = graph;
  mod->data = 0;
  mod->committed_param_size = 0;
  mod->committed_param = 0;
  mod->flags = 0;
  mod->keyframe_cnt = 0;

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
        if(cn->type == dt_token("read") || cn->type == dt_token("sink") || cn->type == dt_token("modify"))
        {
          cn->connected = s_cid_unset;
        }
        else if(cn->type == dt_token("write") || cn->type == dt_token("source"))
        {
          cn->connected.i = 0;
          cn->connected.c = 0;
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
  // only mark as dead. TODO: if we don't like it any
  // more, perform some form of garbage collection.
  assert(modid >= 0 && graph->num_modules > modid);
  if(graph->module[modid].so->cleanup)
    graph->module[modid].so->cleanup(graph->module+modid);
  // disconnect all channels
  for(int c=0;c<graph->module[modid].num_connectors;c++)
  {
    if(dt_connector_input(graph->module[modid].connector+c))
      dt_module_connect(graph, -1, -1, modid, c);
    else
      dt_module_connect(graph, modid, c, -1, -1);
  }
  graph->module[modid].name = 0;
  graph->module[modid].inst = 0;
  graph->module[modid].connector[0].type = 0; // to avoid being detected as sink
  graph->module[modid].flags = 0;
  graph->module[modid].num_connectors = 0;
  free(graph->module[modid].keyframe);
  graph->module[modid].keyframe_size = 0;
  graph->module[modid].keyframe_cnt = 0;
  graph->module[modid].keyframe = 0;
  return 0;
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
    if(graph->module[m].name == 0) continue;
    for(int c=0;c<graph->module[m].num_connectors;c++)
    {
      if(dt_connector_input(graph->module[m].connector+c) &&
         graph->module[m].connector[c].connected.i == modi &&
         graph->module[m].connector[c].connected.c == conn)
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
  if(cout) *cout = us->connector[conn].connected.c;
  return us->connector[conn].connected.i;
}

void dt_module_reset_params(dt_module_t *mod)
{
  if(mod->name == 0) return; // skip deleted modules
  for(int p=0;p<mod->so->num_params;p++)
  {
    dt_ui_param_t *param = mod->so->param[p];
    memcpy(mod->param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
  }
}

void dt_module_keyframe_post_update(dt_module_t *mod)
{
  if(mod->name == 0) return; // skip deleted modules
  if(mod->keyframe_cnt == 0) return; // no keyframes
  for(uint32_t p=0;p<mod->so->num_params;p++)
  {
    mod->param_keyframe[p] = 0xffff;
    for(uint32_t k=0;k<mod->keyframe_cnt;k++)
    {
      if(mod->keyframe[k].param == mod->so->param[p]->name)
      {
        if(mod->param_keyframe[p] == 0xffff)
        { // no keyframe in the chain yet, set first one:
          mod->param_keyframe[p] = k;
          mod->keyframe[k].next = 0;
        }
        else
        { // insert into chain in the right place
          dt_keyframe_t *key = mod->keyframe + mod->param_keyframe[p];
          while(key->next && key->frame > mod->keyframe[k].frame)
            key = key->next;
          // now key->frame is <= than ours for the first time, or we reached the end of the list
          mod->keyframe[k].next = key->next;
          key->next = mod->keyframe+k;
        }
      }
    }
  }
}
