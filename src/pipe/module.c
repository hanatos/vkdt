#include "module.h"


int dt_module_read_params_ascii(
    dt_module_t *mod,
    char *line)
{

}

int dt_module_read_ascii(
    const char *file)
{
  // TODO: read name, instance
  dt_module_add(graph, name, inst);
}

int dt_module_add(
    const dt_module_graph_t *graph,
    const dt_token_t name,
    const dt_token_t inst)
{
  // add to graph's list
  // make sure we have enough memory, realloc if not
  if(graph->num_modules == graph->max_modules)
  {
    assert(0 && "TODO: realloc module graph arrays");
    return -1;
  }
  const int modid = graph->num_modules++;

  dt_module_t *mod = graph->modules+modid;
  mod->name = name;
  mod->inst = inst;

  // TODO: copy over module class!
  dt_module_so_list_t *list = ; // XXX
  for(int i=0;i<list->num_modules;i++)
  { // TODO: hashtable? sort and bisect?
    if(name == list->module[i].name)
    {
      // TODO: slap over defaults and connectors
      // TODO: set connector's module id to ours
      break;
    }
  }
  // TODO: if connectors still empty return -1

  return modid;
}

int dt_module_remove(
    const dt_module_graph_t *graph,
    const int modid)
{
  // TODO: mark as dead and increment dead counter.
  // TODO: if we don't like it any more, call graph_write and graph_read
  // TODO: for garbage collection.
  assert(modid >= 0 && graph->num_modules > modid);
  graph->module[modid].name = 0;
  graph->module[modid].inst = 0;
  dlclose(graph->module[modid].dlhandle);
  graph->module[modid].dlhandle = 0;
}

// return modid or -1
int dt_module_get(
    const dt_module_graph_t *graph,
    const dt_token_t name,
    const dt_token_t inst)
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

// TODO: "templatise" on module and node
int dt_module_connect(
    dt_module_graph_t *graph,
    int m0,  // from
    dt_token_t c0,
    int m1,  // to
    dt_token_t c1)
{
  if(m0 < 0 && m0 >= graph->num_modules) return 1;
  if(m1 > 0 && m1 >= graph->num_modules) return 2;
  // TODO: connect the two in the graph
  dt_connector_t *cn0 = 0, *cn1 = 0;
  for(int k=0;k<10;k++) if(graph->module[m0].connector[k].name == c0)
  {
    cn0 = graph->module[m0].connector+k;
    break;
  }
  if(!cn0) return 3;
  for(int k=0;k<10;k++) if(graph->module[m1].connector[k].name == c1)
  {
    cn1 = graph->module[m1].connector+k;
    break;
  }
  if(!cn1) return 4;

  // check buffer config for compatibility
  if(cn1->type != cn0->type) return 5;
  if(cn1->chan != cn0->chan) return 6;
  if(cn1->format != cn0->format) return 7;

  // connect input id
  cn1->connected_mid = m0;
  cn1->connected_cid = cn0-graph->module[m0].connector;
  // TODO: ??
  cn0->connected_mid = 0; // in general these could be many, so we can't connect like this
  cn0->connected_cid = 0;

  return 0;
}

