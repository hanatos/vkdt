#pragma once
#include "graph.h"
#include <stdio.h>

int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename);

int dt_graph_write_config_ascii(
    dt_graph_t *graph,
    const char *filename);

// TODO: expose individual write to char* functions from io.c file!
// TODO: then in render.cpp, write to graph->history_pool

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
