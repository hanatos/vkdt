#pragma once

// can also be called to compress the edit history
int
dt_graph_history_init(
    dt_graph_t *graph)
{
  graph->history_max = 100<<10;
  graph->history_end = 0;
  graph->history_pool = malloc(sizeof(uint8_t) * graph->history_max);
  graph->history_item_max = 1000;
  graph->history_item_end = 0;
  graph->history_item = malloc(sizeof(uint8_t*) * graph->history_item_max);
  char *max = graph->history_pool + graph->history_max;
  char **hi = graph->history_item;
  int i = 0;
  hi[0] = graph->history_pool;
  // this global block consists of more than one line. need to look for newlines after
  if(!(hi[1] = dt_graph_write_global_ascii(graph, hi[0], max-hi[0]))) return 1;
  const char *tmp = hi[1];
  for(char *c=hi[0];c<tmp;c++) if(*c == '\n') hi[++i] = c+1;

  // write all modules
  for(int m=0;m<graph->num_modules;m++,i++)
    if(!(hi[i+1] = dt_graph_write_module_ascii(graph, m, hi[i], max-hi[i])))
      return 1;

  // write all connections
  for(int m=0;m<graph->num_modules;m++)
    for(int c=0;c<graph->module[m].num_connectors;c++,i++)
      if(!(hi[i+1] = dt_graph_write_connection_ascii(graph, m, c, hi[i], max-hi[i])))
        return 1;

  // write all params
  for(int m=0;m<graph->num_modules;m++)
    for(int p=0;p<graph->module[m].so->num_params;p++,i++)
      if(!(hi[i+1] = dt_graph_write_param_ascii(graph, m, p, hi[i], max-hi[i])))
        return 1;

  // write all keyframes
  for(int m=0;m<graph->num_modules;m++)
    for(int k=0;k<graph->module[m].keyframe_cnt;k++,i++)
      if(!(hi[i+1] = dt_graph_write_keyframe_ascii(graph, m, k, hi[i], max-hi[i])))
        return 1;

  graph->history_item_end = i;
  graph->history_end = hi[i];
  return 0;
}

void
dt_graph_history_cleanup(
    dt_graph_t *graph)
{
  graph->history_item_max = graph->history_item_end = 0;
  free(graph->history_item);
  graph->history_max = graph->history_end = 0;
  free(graph->history_pool);
}

// collect parameter change
// TODO: can we diff it? and store sub params?
// TODO: implement write_paramsub_ascii
void
dt_graph_history_append(
    dt_graph_t *graph,
    int         modid,     // record param of this module
    int         parid,     // record this parameter
    double      throttle)  // throttle same modid,parid to 1 item per `throttle` seconds. pass 0.0 for always record.
{
  static double write_time = 0.0; // TODO: store per graph? i mean throttling is a gui thing, no?
  if(graph->history_item_end >= graph->history_item_max)
  {
    // TODO: resize and copy buffers
    return;
  }
  // this is way conservative (for instance for draw)
  int cnt = graph->module[modid].so->param[parid].cnt
  size_t psz = dt_ui_param_size(graph->module[modid].so->param[parid].type, cnt);
  if(graph->history_max - graph->history_end <= 40+psz)
  { // also need space for param:module:inst:param: prefix
    // TODO: resize and copy buffer, repoint history_item*
    return;
  }
  int i = graph->history_item_end + 1;
  graph->history_item[i] = graph->history_pool + graph->history_end;
  char *eop, *ret = dt_graph_write_param_ascii(graph, modid, parid,
      graph->history_item[i], graph->history_max - graph->history_end, &eop);
  double time = dt_time();
  if(throttle > 0.0 && i > 0 && !strncmp(graph->history_item[i-1], graph->history_item[i], eop-graph->history_item[i]))
    if(time < write_time + throttle) return;
  // now a valid new item:
  graph->history_item_end++;
  graph->history_end = ret - graph->history_pool;
  write_time = time;
}

// TODO: wire these through
void dt_graph_history_module(
    dt_graph_t *graph,
    int         modid)
{
}

void dt_graph_history_connection(
    dt_graph_t *graph,
    int         modid,
    int         conid)
{
}

void dt_graph_history_keyframe(
    dt_graph_t *graph,
    int         modid,
    int         keyid)
{
}

void dt_graph_history_global(
    dt_graph_t *graph)
{
}

// reset graph configuration to a certain point in history
int dt_graph_history_set(
    dt_graph_t *graph,
    int         item)
{
  if(item < 0 || item >= graph->history_item_end) return 1;
  // clean up all connections (they might potentially leave disconnected/broken
  // portions of graph otherwise.
  for(int m=0;m<graph->num_modules;m++)
    for(int c=0;c<graph->module[m].num_connectors;c++)
      if(dt_connector_input(graph->module[m].connector+c))
        dt_module_connect(graph, -1, -1, m, c);
  for(int i=0;i<item;i++)
    if(dt_graph_read_config_line(graph, graph->history_item[i]) < 0)
      return 1;
  return 0;
}
