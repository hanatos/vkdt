#pragma once

#include "token.h"

typedef struct dt_connector_t
{
  dt_token_t name; // name of the connector
  int type;        // TODO: enum for in/out/scratch
  int format;      // TODO: enum/bit masks for RGB, YUV, etc, and 32F, 16UI, ..
}
dt_connector_t;

typedef struct dt_connection_t
{
  dt_token_t out_node; // name of the output node
  dt_token_t out_ndid; // id of the output node
  dt_token_t out_conn; // name of the source in that node
  dt_token_t in_node;  // name of the consumer node
  dt_token_t in_ndid;  // id of the consumer node
  dt_token_t in_conn;  // name of the sink in that node
}
dt_connection_t;

typedef struct dt_node_t
{
  dt_token_t name;         // class of the node, defines shaders and in/outputs
  dt_token_t id;           // id as in multi-instance
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
node_add(dt_token_t name, dt_token_t id);

// connect out node0 -> in node1
// this can fail and the reason will be returned in some enum.
// mismatch of format may be one reason.
// cycles might be another.
int
node_connect(
    dt_token_t name0, dt_token_t id0, dt_token_t conn0,
    dt_token_t name1, dt_token_t id1, dt_token_t conn1);

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
