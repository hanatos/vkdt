#pragma once
#include "token.h"

typedef struct dt_iop_t
{
  VkPipeline            pipeline;
  VkPipelineLayout      layout;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorPool      pool;
}
dt_iop_t;
