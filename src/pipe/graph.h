#pragma once
#include "node.h"
#include "module.h"
#include "alloc.h"

typedef enum dt_graph_run_t
{
  s_graph_run_none           = 0,
  s_graph_run_roi_out        = 1<<0, // pass 1: recompute output roi
  s_graph_run_roi_in         = 1<<1, // pass 2: recompute input roi requests
  s_graph_run_create_nodes   = 1<<2, // pass 2: create nodes from modules
  s_graph_run_alloc_free     = 1<<3, // pass 3: alloc and free images
  s_graph_run_alloc_dset     = 1<<4, // pass 4: alloc descriptor sets and imageviews
  s_graph_run_record_cmd_buf = 1<<5, // pass 4: record command buffer
  s_graph_run_upload_source  = 1<<6, // final : upload new source image
  s_graph_run_download_sink  = 1<<7, // final : download sink images
  s_graph_run_wait_done      = 1<<8, // wait for fence
  s_graph_run_all = -1u,
}
dt_graph_run_t;

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
  uint8_t              *params_pool;
  uint32_t              params_end, params_max;

  // store full history in this block:
  uint8_t              *history_pool;
  uint32_t              history_end, history_max;

  dt_vkalloc_t          heap;           // allocator for device buffers and images
  dt_vkalloc_t          heap_staging;   // used for staging memory, which has different flags

  uint32_t              memory_type_bits;
  uint32_t              memory_type_bits_staging;
  VkDeviceMemory        vkmem;
  VkDeviceMemory        vkmem_staging;
  VkDescriptorPool      dset_pool;
  VkCommandBuffer       command_buffer; // one thread per graph
  VkCommandPool         command_pool;
  VkFence               command_fence;  // one per command buffer

  VkBuffer              uniform_buffer; // uniform buffer shared between all nodes
  VkDeviceMemory        vkmem_uniform;
  uint32_t              uniform_size;
  uint8_t              *uniform_mem;
  VkDescriptorSetLayout uniform_dset_layout;
  VkDescriptorSet       uniform_dset;

  size_t                vkmem_size;
  size_t                vkmem_staging_size;
  size_t                vkmem_uniform_size;

  uint32_t              query_max;
  uint32_t              query_cnt;
  VkQueryPool           query_pool;
  uint64_t             *query_pool_results;
  dt_token_t           *query_name;
  dt_token_t           *query_kernel;

  uint32_t              dset_cnt_image_read;
  uint32_t              dset_cnt_image_write;
  uint32_t              dset_cnt_buffer;
  uint32_t              dset_cnt_uniform;

  dt_graph_run_t        runflags;      // used to trigger next runflags/invalidate things
  int                   lod_scale;     // scale output down by this factor. default = 1.

  // scale output resolution to fit and copy the main display to the given buffer:
  VkImage               thumbnail_image;
  int                   output_wd;
  int                   output_ht;
  void                 *io_mutex;      // if this is set to != 0 will be locked during read_source() calls
}
dt_graph_t;

void dt_graph_init(dt_graph_t *g);
void dt_graph_cleanup(dt_graph_t *g);

dt_node_t *dt_graph_get_display(dt_graph_t *g, dt_token_t  which);

VkResult dt_graph_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run);

void dt_token_print(dt_token_t t);

VkResult dt_graph_create_shader_module(
    dt_token_t node,
    dt_token_t kernel,
    const char *type,   // "comp" "vert" "tesc" "tese" "geom" "frag"
    VkShaderModule *shader_module);
