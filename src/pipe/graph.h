#pragma once
#include "node.h"
#include "module.h"
#include "alloc.h"
#include "raytrace.h"
#ifdef DEBUG_MARKERS
#include "db/db.h"         // for string pool type
#endif

// create nodes requires roi requires alloc requires upload source
typedef uint32_t dt_graph_run_t;

typedef struct dt_connector_image_t
{
  uint64_t      offset, size;   // actual memory position during the time it is valid
  uint64_t      plane1_offset;  // yuv buffers need the offset to the chroma plane
  uint32_t      wd, ht;         // if non zero, these are the varying dimensions of image arrays
  uint32_t      mip_levels;     // number of mip map levels, if any (mostly zero)
  dt_vkmem_t   *mem;            // used for alloc/free during graph traversal
  VkImage       image;          // vulkan image object
  VkImageView   image_view;
  VkImageLayout layout;
  VkBuffer      buffer;         // in case this is a storage buffer
}
dt_connector_image_t;

typedef struct dt_graph_query_t
{
  uint32_t     max;
  uint32_t     cnt;
  VkQueryPool  pool;
  uint64_t    *pool_results;
  dt_token_t  *name;
  dt_token_t  *kernel;
  float        last_frame_duration; // for convenience the last frame time in milliseconds
}
dt_graph_query_t;

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
  char                 *history_pool;
  uint32_t              history_max;
  char                **history_item;
  double               *history_time; // timestamp
  uint32_t              history_item_cur, history_item_end, history_item_max;

  // memory pool for connector allocations
  dt_connector_image_t *conn_image_pool;
  uint32_t              conn_image_end, conn_image_max;

  dt_vkalloc_t          heap;           // allocator for device images
  dt_vkalloc_t          heap_staging;   // used for staging memory, which has different flags

  uint32_t              memory_type_bits;
  uint32_t              memory_type_bits_staging;
  VkDeviceMemory        vkmem;
  VkDeviceMemory        vkmem_staging;
  VkDescriptorPool      dset_pool;
  VkCommandBuffer       command_buffer[2];   // two per graph, to interleave cpu load, uploads and gpu compute
  VkCommandPool         command_pool;
  VkSemaphore           semaphore_display;   // timeline semaphore to keep track of commands that use the backbuffer images
  uint64_t              display_dbuffer[2];  // indicate the largest display timeline position currently reading double buffer 0 or 1
  VkSemaphore           semaphore_process;   // timeline semaphore indicating that graph processing/double buffer write access is done
  uint64_t              process_dbuffer[2];  // timestamps indicating that processing will be done for double buffer 0 or 1
  qvk_queue_name_t      queue_name;

  VkBuffer              uniform_buffer;      // uniform buffer shared between all nodes
  VkDeviceMemory        vkmem_uniform;
  uint32_t              uniform_size;        // size of the total uniform buffer
  uint32_t              uniform_global_size; // size of the global section
  VkDescriptorSetLayout uniform_dset_layout; // same layout for all nodes

  dt_raytrace_graph_t   rt;

  size_t                vkmem_size;          // allocation sizes to tell whether we need to re-alloc
  size_t                vkmem_staging_size;
  size_t                vkmem_uniform_size;

  dt_graph_query_t      query[2];            // for odd and even command buffers, starting at half query_max

  uint32_t              dset_cnt_image_read,  dset_cnt_image_read_alloc;
  uint32_t              dset_cnt_image_write, dset_cnt_image_write_alloc;
  uint32_t              dset_cnt_buffer,      dset_cnt_buffer_alloc;
  uint32_t              dset_cnt_uniform,     dset_cnt_uniform_alloc;

  dt_graph_run_t        runflags;      // used to trigger next runflags/invalidate things
  int                   lod_scale;     // scale output down by this factor. default = 1.
  int                   active_module; // currently active module, relevant for runflags

  int                   frame;
  int                   frame_cnt;     // number of frames to compute
  double                frame_rate;    // frame rate (frames per second)

  // index of the currently written double buffer channel for s_conn_double_buffer connectors.
  // graph_run will pick this value up and use the dsets for writing.
  // this also selects the command buffer/fence for interleaved processing.
  int                   double_buffer;

  // scale output resolution to fit and copy the main display to the given buffer:
  VkImage               thumbnail_image;
  void                 *io_mutex;      // if this is set to != 0 will be locked during read_source() calls

  int                   gui_attached;  // can't free the output images while still used etc.
  const char           *gui_msg;       // will result in a dt_gui_notification call if not zero
  char                  gui_msg_buf[100];
  float                 gui_colour[8]; // rgba of accent and accent hover colours, for some dspy modules
  char                  searchpath[PATH_MAX];

  uint32_t              main_img_hash;
  dt_image_params_t     main_img_param;// will be copied over from the i-*:main module after modify_roi_out
#ifdef DEBUG_MARKERS
  dt_stringpool_t       debug_markers; // store string names of vk objects here
#endif
}
dt_graph_t;

void dt_graph_init(dt_graph_t *g, qvk_queue_name_t qname); // init
void dt_graph_cleanup(dt_graph_t *g);                      // cleanup, free memory
void dt_graph_reset(dt_graph_t *g);                        // lightweight reset, keep allocations

dt_node_t *dt_graph_get_display(dt_graph_t *g, dt_token_t  which);

VkResult dt_graph_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run);

void dt_token_print(dt_token_t t);

VkResult dt_graph_create_shader_module(
    dt_graph_t     *graph,  // only needed for debugging purposes
    dt_token_t      node,
    dt_token_t      kernel,
    const char     *type,   // "comp" "vert" "tesc" "tese" "geom" "frag"
    VkShaderModule *shader_module);

// return the memory allocation and VkImage etc corresponding to a node's connector
dt_connector_image_t*
dt_graph_connector_image(
    dt_graph_t *graph,
    int         nid,    // node id
    int         cid,    // connector id
    int         array,  // array index
    int         dbuf);  // double buffer index

// apply all keyframes found in the module list and write to the modules parameters according to
// the current frame in the graph (g->frame). floating point parameters will be interpolated.
void dt_graph_apply_keyframes(
    dt_graph_t *g);

static inline dt_token_t
dt_node_get_instance(
    dt_node_t *node)
{
  return node->module->inst;
}

static inline dt_token_t
dt_module_get_instance(
    dt_module_t *module)
{
  return module->inst;
}
