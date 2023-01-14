#pragma once

// detect cycles in the graph before actually performing the connection

// recursive helper
static inline int
dt_connection_is_cyclic_rec(
    dt_graph_t *g,
    const int   mid,
    int        *visited,
    int        *stack,
    const int   m0_extra)
{
  if(mid < 0) return 0;
  if(!visited[mid])
  {
    visited[mid] = 1;
    stack  [mid] = 1;
    if(m0_extra >= 0)
    { // test extra connection that is not actually on the graph yet:
      if(!visited[m0_extra] && dt_connection_is_cyclic_rec(g, m0_extra, visited, stack, -1))
        return 1;
      else if(stack[m0_extra])
        return 1;
    }
    for(int c=0;c<g->module[mid].num_connectors;c++)
    {
      dt_connector_t *cn = g->module[mid].connector+c;
      if((cn->type == dt_token("read") || cn->type == dt_token("sink")) &&
        !(cn->flags & s_conn_feedback))
      { // don't visit feedback connectors, these don't constitute cycles
        int m0 = cn->connected_mi;
        if(m0 < 0) continue; // disconnected, may be fine
        if(!visited[m0] && dt_connection_is_cyclic_rec(g, m0, visited, stack, -1))
          return 1;
        else if(stack[m0])
          return 1;
      }
    }
  }
  stack[mid] = 0;
  return 0;
}

// we know the graph has no cycles.
// the only one can occur due to the new connection
// so we need to pull only on the m1 module
static inline int
dt_connection_is_cyclic(
    dt_graph_t *g,
    const int   m0,
    const int   c0,
    const int   m1,
    const int   c1)
{
  int visited[200] = {0};
  int stack  [200] = {0};
  assert(g->num_modules < sizeof(visited)/sizeof(visited[0]));
  // if(!visited[m1] &&// only testing one, cannot happen
  // in the general case need to test for all modules
  if(dt_connection_is_cyclic_rec(g, m1, visited, stack, m0))
    return 1;
  return 0;
}

static inline int
dt_graph_is_cyclic(
    dt_graph_t *g)
{
  int visited[200] = {0};
  int stack  [200] = {0};
  assert(g->num_modules < sizeof(visited)/sizeof(visited[0]));
  for(int m=0;m<g->num_modules;m++)
  {
    if(!g->module[m].name) continue;
    if(!visited[m] && dt_connection_is_cyclic_rec(g, m, visited, stack, -1))
      return m;
  }
  return -1;
}
