#include "module.h"

int dt_module_so_load(
    dt_token_t name)
{
  // TODO: init connectors from name/module file
  // TODO: read parameter configuration and defaults from name/params
  // TODO: dlopen() callback functions
  char obj[128] = {0};
  sprintf(obj, "lib%" PRItkn ".so", name);
  mod->dlhandle = dlopen(obj, RTLD_LAZY | RTLD_LOCAL);
  if(dlerror())
  {
    // TODO:
    return -1;
  }
  mod->create_nodes = dlsym(mod->dlhandle, "create_nodes");
  if(dlerror())
  {
    // TODO
  }

  // TODO: read default params

  // TODO: read connector info
  // open mod->name"/connectors"
  for(connectors)
  {
  dt_token_t      name;   // connector name
  // TODO: make these tokens, too?
  dt_con_type_t   type;   // read write source sink
  dt_buf_type_t   chan;   // rgb yuv..
  dt_buf_format_t format; // f32 ui16
  modid = -1;
  }

      // TODO: in some cleanup:
      // decrements ref count and unloads the dso if it drops to 0
  dlclose(mod->dlhandle);
}

// reads one line of a connector configuration file.
// this is: connector name, type, channels, format
// as four tokens with ':' as separator.
int dt_module_read_connector_ascii(
    dt_connector_t *conn,
    char *line)
{
  dt_token_t tkn[4] = {0};
  char *c = line, *c0 = line;
  for(int i=0;i<4;i++)
  {
    while(*c != ':' && *c != '\n' && c < c0+8) c++;
    assert((i == 4 && *c = '\n') || (i != 4 && *c == ':'));
    // TODO: some error message/code and return 1
    *c = 0;
    tkn[i] = dt_token(c0);
    c0 = c;
  }
  conn->name = tkn[0];
  conn->type = tkn[1];
  conn->chan = tkn[2];
  conn->format = tkn[3];
  return 0;
}

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

// TODO return what on failure?
int dt_module_connect(
    dt_module_graph_t *graph,
    int modid0,
    int modid1)
{
  // TODO: connect the two in the graph
}

