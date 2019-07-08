#pragma once
#include "graph.h"

int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename);

int dt_graph_write_config_ascii(
    dt_graph_t *graph,
    const char *filename);

