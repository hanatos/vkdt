#pragma once
#include "node.h"
#include "module.h"
#include "alloc.h"

// the graph is stored as list of modules and list of nodes.
// these have connectors with detailed buffer information which
// also hold the id to the other connected module or node. thus,
// there is no need for an explicit list of connections.
//
// one graph is run by one thread, so it encapsulates all necessary
// multithreading things for the vulkan backend (has it's own command pool for
// instance)
typedef struct dt_graph_t
{
  dt_module_t *module;
  uint32_t num_modules, max_modules;

  dt_node_t *node;
  uint32_t num_nodes, max_nodes;

  // memory pool for node params. this is a simple allocator that increments
  // the end pointer until it is flushed completely.
  uint8_t *params_pool;
  uint32_t params_end, params_max;

  // TODO: also store full history somewhere

  dt_vkalloc_t heap;           // allocator for device buffers and images
  dt_vkalloc_t heap_staging;   // used for staging memory, which has different flags

  uint32_t memory_type_bits, memory_type_bits_staging;
  VkDeviceMemory        vkmem;
  VkDeviceMemory        vkmem_staging;
  VkDescriptorPool      dset_pool;
  VkCommandBuffer       command_buffer; // we might have may buffers to interleave them (thumbnails?)
  VkCommandPool         command_pool;   // but we definitely need one pool for ourselves (our thread)
  VkFence               command_fence;  // one per command buffer

  VkBuffer              uniform_buffer; // uniform buffer shared between all nodes
  VkDeviceMemory        vkmem_uniform;
  uint32_t              uniform_size;
  VkDescriptorSetLayout uniform_dset_layout;
  VkDescriptorSet       uniform_dset;

  uint32_t dset_cnt_image_read;
  uint32_t dset_cnt_image_write;
  uint32_t dset_cnt_buffer;
  uint32_t dset_cnt_uniform;
}
dt_graph_t;

void dt_graph_init(dt_graph_t *g);
void dt_graph_cleanup(dt_graph_t *g);

// read modules from ascii or binary
int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename);

// write only modules connected to sink modules,
// and only set parameters of modules once.
int dt_graph_write_compressed(
    dt_graph_t *graph,
    const char *filename);

// write everything, including full history?
int dt_graph_write(
    dt_graph_t *graph,
    const char *filename);

void dt_graph_setup_pipeline(
    dt_graph_t *graph);

void dt_token_print(dt_token_t t);
