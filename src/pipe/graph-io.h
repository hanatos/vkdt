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
