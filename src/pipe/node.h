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

// api interface: connect(n0, id0, out, n1, id1, in)

// TODO: create node classes from description files (see contrast.c)
