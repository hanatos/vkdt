#pragma once
#include "token.h"
#include "connector.h"

typedef struct dt_node_t
{
  dt_connector_t connector[10];
  int num_connectors;
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

// api interface:

// add new node in the pipeline, construct from class given by name
// and with given multi-instance id
void
node_add(dt_token_t module, dt_token_t name, dt_token_t id);
