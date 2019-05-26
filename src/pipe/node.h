#pragma once
#include "token.h"
#include "connector.h"

#include <vulkan/vulkan.h>

typedef struct dt_node_t
{
  dt_token_t name;
  dt_token_t kernel;

  dt_connector_t connector[DT_MAX_CONNECTORS];
  int num_connectors;

  VkPipeline            pipeline;
  VkPipelineLayout      pipeline_layout;
  VkDescriptorSet       dset;
  VkDescriptorSetLayout dset_layout;
}
dt_node_t;

