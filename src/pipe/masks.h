#pragma once
#include "graph.h"

#include <vulkan/vulkan.h>

VkResult dt_masks_create_pipeline(
    dt_graph_t *graph,   // associated graph
    dt_node_t  *node,    // node to create draw call pipeline for
    const int   ci);     // connector index with output framebuffer

void dt_masks_record_draw(
    dt_graph_t *graph,
    dt_node_t  *node,   // node with draw kernels
    const int   ci);    // output connector index

void dt_masks_cleanup(
    dt_graph_t *graph,
    dt_node_t  *node,
    const int   ci);
