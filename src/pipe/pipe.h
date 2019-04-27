#pragma once
#include "token.h"

// every module is run with their own tiling, to match a fixed-size ping/pong
// buffer allocation. every module gets two input buffers: one full roi low res
// (preview) and one full res cropped roi.

// see vkrun.h:
// need vulkan pipeline
// need buffers:
// - preview input/output
// - roi input / output
// cached input for active module
// 2 descriptor sets for ping pong buffer

// pipeline setup:
// set memory size/ping pong buffer dimensions
// ask modules for required temp buffers and allocate
// set up vulkan command buffer and pipeline:
// vkCreateComputePipelines with many info objects (one per module)

// 1) output requests certain roi+scale
// 2) this pulls rois from all input modules
// 3) rois are processed from input to output,
//    respecting the fixed buffer size and running
//    on tiles as required (this step needs to be run for every tile and the
//    tile size depends on the most restrictive module on the way)
//
// tiling: 
// every module first runs the preview buffer (as tmp results may be needed by
// the full buffer). then the current active tile is processed and handed over
// to the next module. this means tiling reprocesses the preview buffers :(

// module interface:
// get_roi_in()
// get_roi_out()
// something_add_to_pipeline() { returns temp buffers + spirv }
//   these tmp buffer requests will be merged with other modules to reuse
//   allocations


// simple version for thumbnails and full-res export:
// no extra preview pipeline pass, just allocate largest necessary buffer
// and pump roi through it

typedef struct dt_iop_t
{
  // pipeline: see vkrun.h for how to fill this.
  VkPipeline            pipeline;
  VkPipelineLayout      layout;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorPool      pool;
  // TODO: need one pipeline for each shader kernel in the module.
  // this would be 6 for local laplacian

  // TODO: use push constants for iop params?
}
dt_iop_t;

typedef struct dt_iop_api_t
{
  // function pointers for:
  // create pipeline
  // create temp buffers + descriptor sets?
}
dt_iop_api_t;


// multiple inputs:
// a module may ask for multiple input:
// - cached masks?
// - huge luts from disk
// - other images for denoising/alignment
// this may in fact turn the module api into a full blown node graph.


// buffer management:
// we'll need to vkAllocateMemory() for all our vkCreateBuffer() and then vkBindBufferMemory.
// vulkan allows us to excessively alias these allocations, so we'll create buffers as we
// need them for each module and bind them all to the same chunks of memory.
// as an alternative we might create /one/ temp buffer and use offsets as push constants
// to deliver chunks to the modules. this seems to be the most efficient way to do it.
//
// 1) first pass through all modules, asking for mem requirements
// 2) allocate big chunk
// 3) bind buffers for modules, replace outputs for inputs, alias the tmp bufs
//    in this step also bind the preview pipe buffers as input to the module
