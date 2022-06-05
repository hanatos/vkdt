#pragma once
#include "graph.h"
#include <stdio.h>

int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename);

int dt_graph_write_config_ascii(
    dt_graph_t *graph,
    const char *filename);

// write module definition
char *
dt_graph_write_module_ascii(
    const dt_graph_t *graph,
    const int         m,
    char             *line,
    size_t            size);

// serialises only incoming connections (others can't be resolved due to ambiguity)
char *
dt_graph_write_connection_ascii(
    const dt_graph_t *graph,
    const int         m,      // module index
    const int         i,      // connector index on given module
    char             *line,
    size_t            size);

// write param
char *
dt_graph_write_param_ascii(
    const dt_graph_t *graph,
    const int         m,
    const int         p,
    char             *line,
    size_t            size,
    char            **eop);   // end of prefix: where does the data start after module:inst:param:

// write keyframe
char *
dt_graph_write_keyframe_ascii(
    const dt_graph_t *graph,
    const int         m,      // module id
    const int         k,      // keyframe id
    char             *line,
    size_t            size);

// write globals (frame, fps)
char *
dt_graph_write_global_ascii(
    const dt_graph_t *graph,
    char             *line,
    size_t            size);

int dt_graph_read_config_line(
    dt_graph_t *graph,
    char *line);

int dt_graph_set_searchpath(
    dt_graph_t *graph,
    const char *filename);

// read an extra block of modules to append to the graph
int dt_graph_read_block(
    dt_graph_t *graph,
    const char *filename,  // block cfg to load
    dt_token_t inst,       // instance to drop into new modules
    dt_token_t out_mod,    // output of graph, to be connected to input of block
    dt_token_t out_inst,
    dt_token_t out_conn,
    dt_token_t in_mod,     // output of block to be connected to this input
    dt_token_t in_inst,
    dt_token_t in_conn);
