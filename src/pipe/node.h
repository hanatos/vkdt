#pragma once

#include "token.h"

typedef enum dt_con_type_t
{
  s_invalid_con_type = 0,
  s_output = 1,
  s_tmp = 2,
}
dt_con_type_t;

typedef enum dt_buf_type_t
{
  s_invalid_buf_type = 0,
  s_rggb = 1,   // regular rggb bayer pattern, one channel per pixel
  s_rgbx = 2,   // x-trans one channel per pixel
  s_rgb = 3,    // rgb three channels per pixel
  s_yuv = 4,    // yuv
  s_y = 5,      // only y
  s_mask = 6,   // single channel mask info
}
dt_con_type_t;

typedef enum dt_buf_format_t
{
  s_invalid_format = 0,
  s_f32 = 1,
  s_16ui = 2,
}
dt_buf_format_t;

typedef struct dt_connector_t
{
  dt_token_t name;        // name of the connector
  dt_con_type_t type;     // input output?
  dt_buf_type_t chan;     // channel types rggb, rgb, y, ..
  dt_buf_format_t format; // storage format 32f, 16ui, ..
}
dt_connector_t;

// TODO: maintain global list of such nodes
typedef struct dt_node_id_t
{
  dt_token_t module; // module directory
  dt_token_t node;   // node inside this module
  dt_token_t id;     // id for multi instance
}
dt_node_id_t;

typedef struct dt_connection_t
{
  dt_node_id_t out_node; // output node
  dt_token_t   out_conn; // name of the source in that node
  dt_node_id_t in_node;  // consumer node
  dt_token_t   in_conn;  // name of the sink in that node
  // TODO: colour space annotation, bayer pattern offsets, etc goes in here?
}
dt_connection_t;

typedef struct dt_node_t
{
  dt_node_id_t nodeid;
  dt_connector_t in[10];
  dt_connector_t out[10];
  dt_connector_t tmp[10];
  // TODO: store counts?
  // TODO: vulkan temporaries, pipeline etc?
}
dt_node_t;

typedef struct dt_module_t
{
  dt_node_t *node;       // every module can have multiple nodes (for instance wavelets decompose, process, synthesise)

  // TODO: params: see contrast.c for an example:
  dt_token_t *param_tkn; // token for id
  float      *param_val; // float values to be copied as uniforms in this layout
  float      *param_cnt; // number of values for each param
  const char *param_str; // translatable string for each param

  // TODO: gui annotations: 
  float *param_min, *param_max;

  // need special case callbacks, optionally:
  // TODO: init() draw_gui(), ..? for custom gui
  // load_input(void *); // this is a source node, gets the vkMapMemory mapped pointer to fill
  // write_output(void *); // this is a sink node
}
dt_module_t;

// TODO: store list of nodes and list of connections
// TODO: do topological sort for gui and for command buffer ingestion?
// TODO: use vk events to synch

// TODO: read from pipeline configuration input file:
// node list:
// class id
// connection list:
// name:id:output -> name:id:input
// note again that these are all uint64_t tokens which can come in a binary format
// for faster parsing/ingestion

// api interface:

// add new node in the pipeline, construct from class given by name
// and with given multi-instance id
void
node_add(dt_token_t module, dt_token_t name, dt_token_t id);

// connect out node0 -> in node1
// this can fail and the reason will be returned in some enum.
// mismatch of format may be one reason.
// cycles might be another.
int
node_connect(
    dt_token_t mod0, dt_token_t name0, dt_token_t id0, dt_token_t conn0,
    dt_token_t mod1, dt_token_t name1, dt_token_t id1, dt_token_t conn1);

// read params:
int
node_params_read(
    dt_token_t name, dt_token_t id,
    // some input format such as void* or FILE*
    );

// TODO: create node classes from description files
// load node class (see contrast.c for a stub)
int
node_load(dt_token_t name)
{
  // TODO: load connectors (name, type, format)
  // TODO: load param config + defaults:
  //       (name, translatable string, len, args as float array)
  // TODO: load gui annotations. for now everything is a slider?
  //       need to specify bounds, softbounds, step size, log space?
}

// serialisation:
// (ascii or binary)
// node_write()
// node_connection_write()
// node_params_write()
// nodes classes themselves are hand-written
