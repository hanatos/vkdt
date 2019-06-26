#pragma once
#include "pipe/graph.h"


static inline void
dt_graph_print_modules(
    const dt_graph_t *graph)
{
  fprintf(stdout, "digraph modules {\nnode [shape=record]\n");
  // for all modules, print all incoming edges (outgoing don't have module ids)
  for(int m=0;m<graph->num_modules;m++)
  {
    fprintf(stdout, "n%d_%"PRItkn" [label=\"%"PRItkn"|{",
        m,
        dt_token_str(graph->module[m].name),
        dt_token_str(graph->module[m].name));
    for(int c=0;c<graph->module[m].num_connectors;c++)
    {
      fprintf(stdout, "<%d> %"PRItkn, c, dt_token_str(graph->module[m].connector[c].name));
      if(c != graph->module[m].num_connectors - 1)
        fprintf(stdout, "|");
      else fprintf(stdout, "}\"];\n");
    }
  }
  for(int m=0;m<graph->num_modules;m++)
  {
    for(int c=0;c<graph->module[m].num_connectors;c++)
    {
      if((graph->module[m].connector[c].type == dt_token("read") ||
            graph->module[m].connector[c].type == dt_token("sink")) &&
          graph->module[m].connector[c].connected_mi >= 0)
      {
        fprintf(stdout, "n%d_%"PRItkn":%d -> n%d_%"PRItkn":%d\n",
            graph->module[m].connector[c].connected_mi,
            dt_token_str(graph->module[
              graph->module[m].connector[c].connected_mi
            ].name),
            graph->module[m].connector[c].connected_mc,
            m,
            dt_token_str(graph->module[m].name),
            c);
      }
    }
  }
  fprintf(stdout, "}\n");
}

static inline void
dt_graph_print_nodes(
    const dt_graph_t *graph)
{
  fprintf(stdout, "digraph nodes {\nnode [shape=record]\n");
  // for all nodes, print all incoming edges (outgoing don't have module ids)
  for(int m=0;m<graph->num_nodes;m++)
  {
    fprintf(stdout, "n%d_%"PRItkn"_%"PRItkn
        " [label=\"%"PRItkn"_%"PRItkn"|{",
        m,
        dt_token_str(graph->node[m].name),
        dt_token_str(graph->node[m].kernel),
        dt_token_str(graph->node[m].name),
        dt_token_str(graph->node[m].kernel));
    for(int c=0;c<graph->node[m].num_connectors;c++)
    {
      fprintf(stdout, "<%d> %"PRItkn, c, dt_token_str(graph->node[m].connector[c].name));
      if(c != graph->node[m].num_connectors - 1)
        fprintf(stdout, "|");
      else fprintf(stdout, "}\"];\n");
    }
  }
  for(int m=0;m<graph->num_nodes;m++)
  {
    for(int c=0;c<graph->node[m].num_connectors;c++)
    {
      if((graph->node[m].connector[c].type == dt_token("read") ||
            graph->node[m].connector[c].type == dt_token("sink")) &&
          graph->node[m].connector[c].connected_mi >= 0)
      {
        fprintf(stdout, "n%d_%"PRItkn"_%"PRItkn":%d -> n%d_%"PRItkn"_%"PRItkn":%d\n",
            graph->node[m].connector[c].connected_mi,
            dt_token_str(graph->node[
              graph->node[m].connector[c].connected_mi
            ].name),
            dt_token_str(graph->node[
              graph->node[m].connector[c].connected_mi
            ].kernel),
            graph->node[m].connector[c].connected_mc,
            m,
            dt_token_str(graph->node[m].name),
            dt_token_str(graph->node[m].kernel),
            c);
      }
    }
  }
  fprintf(stdout, "}\n");
}
