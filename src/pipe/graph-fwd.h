#pragma once

#define DT_GRAPH_MAX_FRAMES 2
typedef uint32_t dt_graph_run_t;
typedef struct dt_module_t dt_module_t;
typedef struct dt_node_t dt_node_t;
typedef struct dt_graph_t dt_graph_t;

typedef enum dt_module_flags_t
{
  s_module_request_none        = 0,
  s_module_request_read_source = 1,
  s_module_request_write_sink  = 2,
  s_module_request_build_bvh   = 4,
  s_module_request_dyn_array   = 8,
  s_module_request_all         = 16,
}
dt_module_flags_t;

