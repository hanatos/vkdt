#pragma once
#include "token.h"
#include "connector.h"

typedef struct dt_node_t
{
  dt_token_t name;
  dt_token_t kernel;

  dt_connector_t connector[10];
  int num_connectors;
  // TODO: vulkan temporaries, pipeline etc?
}
dt_node_t;

