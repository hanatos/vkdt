#include "module.h"


// this is a public api function
int dt_module_add(
    const dt_graph_t *graph,
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
      // TODO: set connector's ref id's to -1 or ref count to 0 if a write|source node
      break;
    }
  }
  // TODO: if connectors still empty return -1

  return modid;
}

int dt_module_get_connector(
    const dt_module_t *m, dt_token_t conn)
{
  for(int c=0;c<m->num_connectors;c++)
    if(m->connector[c].name == conn) return c;
  return -1;
}

int dt_module_remove(
    dt_graph_t *graph,
    const int modid)
{
  // TODO: mark as dead and increment dead counter. if we don't like it any
  // more, perform some for garbage collection.
  assert(modid >= 0 && graph->num_modules > modid);
  // disconnect all channels
  for(int c=0;c<graph->module[modid].num_channels;c++)
    dt_module_connect(graph, modid, c, -1, -1);
  graph->module[modid].name = 0;
  graph->module[modid].inst = 0;
}

// return modid or -1
int dt_module_get(
    const dt_graph_t *graph,
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

