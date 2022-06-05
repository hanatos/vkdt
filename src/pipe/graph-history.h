#pragma once
#include "pipe/graph-io.h"

static inline void
dt_graph_history_init(
    dt_graph_t *graph)
{
  graph->history_max = 100<<10;
  graph->history_pool = malloc(sizeof(uint8_t) * graph->history_max);
  graph->history_item_max = 1000;
  graph->history_item_end = 0;
  graph->history_item = malloc(sizeof(uint8_t*) * graph->history_item_max);
}

static inline int
dt_graph_history_reset(
    dt_graph_t *graph)
{
  char *tmp, *max = graph->history_pool + graph->history_max;
  char **hi = graph->history_item;
  int i = 0;
  hi[0] = graph->history_pool;
  // this global block consists of more than one line. need to look for newlines after
  if(!(tmp = dt_graph_write_global_ascii(graph, hi[0], max-hi[0]))) return 1;
  for(char *c=hi[0];c<tmp;c++) if(*c == '\n') hi[++i] = c+1;

  // write all modules
  for(int m=0;m<graph->num_modules;m++,i+=(hi[i+1]!=hi[i]))
    if(!(hi[i+1] = dt_graph_write_module_ascii(graph, m, hi[i], max-hi[i])))
      return 1;

  // write all connections
  for(int m=0;m<graph->num_modules;m++)
    for(int c=0;c<graph->module[m].num_connectors;c++,i+=(hi[i+1]!=hi[i]))
      if(!(hi[i+1] = dt_graph_write_connection_ascii(graph, m, c, hi[i], max-hi[i])))
        return 1;

  // write all params
  for(int m=0;m<graph->num_modules;m++)
    for(int p=0;p<graph->module[m].so->num_params;p++,i+=(hi[i+1]!=hi[i]))
      if(!(hi[i+1] = dt_graph_write_param_ascii(graph, m, p, hi[i], max-hi[i], 0)))
        return 1;

  // write all keyframes
  for(int m=0;m<graph->num_modules;m++)
    for(int k=0;k<graph->module[m].keyframe_cnt;k++,i+=(hi[i+1]!=hi[i]))
      if(!(hi[i+1] = dt_graph_write_keyframe_ascii(graph, m, k, hi[i], max-hi[i])))
        return 1;

  graph->history_item_end = i;
  return 0;
}

static inline void
dt_graph_history_cleanup(
    dt_graph_t *graph)
{
  graph->history_max = graph->history_item_max = graph->history_item_end = 0;
  free(graph->history_item); graph->history_item = 0;
  free(graph->history_pool); graph->history_pool = 0;
}

static inline int
_dt_graph_history_check_buf(
    dt_graph_t *graph,
    size_t      size)
{
  if(graph->history_item_end >= graph->history_item_max)
  { // TODO: resize and copy buffers
    return 1;
  }
  if(graph->history_pool + graph->history_max - graph->history_item[graph->history_item_end] <= size)
  { // TODO: resize and copy buffer, repoint history_item*
    return 1;
  }
  return 0;
}

// collect parameter change
static inline void
dt_graph_history_append(
    dt_graph_t *graph,
    int         modid,     // record param of this module
    int         parid,     // record this parameter
    double      throttle)  // throttle same modid,parid to 1 item per `throttle` seconds. pass 0.0 for always record.
{
  static double write_time = 0.0; // does not need to go on graph, throttling is a gui thing.
  int cnt = graph->module[modid].so->param[parid]->cnt; // this is way conservative (for instance for draw)
  size_t psz = dt_ui_param_size(graph->module[modid].so->param[parid]->type, cnt);
  if(_dt_graph_history_check_buf(graph, 40+psz)) return;
  int i = graph->history_item_end;
  char *eop, **hi = graph->history_item, *max = graph->history_pool + graph->history_max;
  if(hi[i] < (hi[i+1] = dt_graph_write_param_ascii(graph, modid, parid, hi[i], max - hi[i], &eop)))
  {
    double time = dt_time();
    if(throttle > 0.0 && i > 0 && !strncmp(hi[i-1], hi[i], eop-hi[i]))
      if(time < write_time + throttle) return;
    // now a valid new item:
    graph->history_item_end++;
    write_time = time;
  }
}

static inline void
dt_graph_history_module(
    dt_graph_t *graph,
    int         modid)
{
  if(_dt_graph_history_check_buf(graph, 30)) return;
  int i = graph->history_item_end;
  char **hi = graph->history_item, *max = graph->history_pool + graph->history_max;
  if(hi[i] < (hi[i+1] = dt_graph_write_module_ascii(graph, modid, hi[i], max - hi[i])))
    graph->history_item_end++;
}

void dt_graph_history_connection(
    dt_graph_t *graph,
    int modid, int conid)
{
  if(_dt_graph_history_check_buf(graph, 70)) return;
  int i = graph->history_item_end;
  char **hi = graph->history_item, *max = graph->history_pool + graph->history_max;
  if(hi[i] < (hi[i+1] = dt_graph_write_connection_ascii(graph, modid, conid, hi[i], max - hi[i])))
    graph->history_item_end++;
}

void dt_graph_history_keyframe(
    dt_graph_t *graph,
    int         modid,
    int         keyid)
{
  dt_module_t *mod = graph->module+modid;
  int beg = mod->keyframe[keyid].beg, end = mod->keyframe[keyid].end;
  int p = dt_module_get_param(mod->so, mod->keyframe[keyid].param);
  size_t psz = dt_ui_param_size(mod->so->param[p]->type, end-beg);
  if(_dt_graph_history_check_buf(graph, 70+psz)) return;
  int i = graph->history_item_end;
  char **hi = graph->history_item, *max = graph->history_pool + graph->history_max;
  if(hi[i] < (hi[i+1] = dt_graph_write_keyframe_ascii(graph, modid, keyid, hi[i], max - hi[i])))
    graph->history_item_end++;
}

void dt_graph_history_global(
    dt_graph_t *graph)
{
  if(_dt_graph_history_check_buf(graph, 40)) return;
  int i = graph->history_item_end;
  char *tmp, **hi = graph->history_item, *max = graph->history_pool + graph->history_max;
  if(!(tmp = dt_graph_write_global_ascii(graph, hi[i], max-hi[i]))) return;
  for(char *c=hi[i];c<tmp;c++) if(*c == '\n') hi[++i] = c+1;
  graph->history_item_end = i;
}

// reset graph configuration to a certain point in history
static inline int
dt_graph_history_set(
    dt_graph_t *graph,
    int         item)
{
  if(item < 0 || item >= graph->history_item_end) return 1;
  // clean up all connections (they might potentially leave disconnected/broken
  // portions of graph otherwise).
  for(int m=0;m<graph->num_modules;m++)
    for(int c=0;c<graph->module[m].num_connectors;c++)
      if(dt_connector_input(graph->module[m].connector+c))
        dt_module_connect(graph, -1, -1, m, c);
  for(int i=0;i<item;i++)
    if(dt_graph_read_config_line(graph, graph->history_item[i]) < 0)
      return 1;
  return 0;
}
