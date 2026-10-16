#pragma once
// double buffering / display queue interfacing

// cleanup:
// TODO only do mipmaps here, not in graph (don't assign s_conn_mipmap and s_conn_concurrent)
// TODO don't double buffer displays in the graph (don't assign s_conn_double_buffer and don't propagate)
// TODO remove graph_res?

// this almost works like s_conn_double_buffer, only that it destroys/inits the resources
// for the double buffer in question only when the command buffer is recorded. the current
// method frees all memory (both double buffers) in case the graph topology is changed.
// this out-of-band buffer here allows the render thread to display the output images
// for as long as it has to.

// the render thread can only lock one of the buffers (only one submit in flight)
// the other should be free for us to delete/overwrite.
// we need a triple buffer for the worst case:
//  (a) one buffer in use by render dispatch (from older graph config, different size, say)
//  (b) one buffer in use by graph dispatch  (new graph config)
//  (c) one buffer needs to be allocated by the main thread when recording the command buffer
// note that we don't have to keep them around indefinitely and can clean up when appropriate.
// for instance, once the render dispatch finished, (a) can be cleaned out in favour of (c)
// and regular operation requires only two buffers.

typedef struct dt_graph_display_image_t
{
  dt_connector_image_t con;    // fake connector image which holds image + views and mipmap info
  VkDescriptorSet      dset;
  VkDeviceMemory       mem;
  dt_token_t           format; // e.g. f16
  dt_token_t           chan;   // e.g. rgba
  uint64_t             val;    // global timeline semaphore value.
}
dt_graph_display_image_t;

typedef enum dt_graph_display_t
{
  s_graph_display_main  = 0,
  s_graph_display_hist,
  s_graph_display_dspy,
  s_graph_display_view0,
  s_graph_display_view1,
  s_graph_display_cnt,
}
dt_graph_display_t;

typedef struct dt_graph_display_images_t
{
  uint64_t timeline_display; // this is the image currently used by display/ui dispatches
  uint64_t timeline_process; // this is the image currently in flight for graph dispatch (>=timeline_display, == if done and not rendering)
  VkDescriptorPool dset_pool;
  dt_graph_display_image_t display_image[s_graph_display_cnt][3];

  VkPipeline            readback_pipeline;
  VkPipelineLayout      readback_pipeline_layout;
  VkDescriptorSetLayout readback_dset_layout;
}
dt_graph_display_images_t;

VkResult dt_graph_display_images_init(dt_graph_display_images_t *dspy);
void dt_graph_display_images_cleanup(dt_graph_display_images_t *dspy);

void dt_graph_display_image_cleanup(
    dt_graph_display_images_t *dspy,
    dt_graph_display_image_t  *d);

int // return 1 if display image is compatible with given display node
dt_graph_display_image_check_compat(
    dt_graph_display_image_t *d,
    dt_graph_t               *graph, // for the dset_pool
    dt_node_t                *node); // the display node

VkResult
dt_graph_display_image_init(
    dt_graph_display_image_t *d,
    dt_graph_t               *graph, // for the dset_pool
    dt_node_t                *node); // the display node

void
dt_graph_display_image_cmd_copy(
    dt_graph_display_image_t *d,     // pointing to the display image to write to
    uint64_t                  val,   // timeline value to set on d
    dt_graph_t               *graph, // the graph with the descriptor set pool
    dt_node_t                *node); // the display node

int // return 1 if image with this timeline value is ready for display
dt_graph_display_image_ready_for_display(
    dt_graph_t *g,
    uint64_t    val);

uint64_t 
dt_graph_display_acquire_for_display(
    dt_graph_t *g);

uint64_t 
dt_graph_display_acquire_for_processing(
    dt_graph_t *graph);

int // return 1 if at least one of the two gpu processes is idle and could accept a new graph_run() call
dt_graph_display_have_free_process(
    dt_graph_t *graph);

VkDescriptorSet
dt_graph_display_get_dset(
    dt_graph_t *g,
    dt_token_t  name);

VkResult // write current display as new thumbnail on disk
dt_graph_display_write_thumbnail();
