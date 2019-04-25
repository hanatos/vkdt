#pragma once

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
