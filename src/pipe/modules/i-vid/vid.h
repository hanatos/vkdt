#pragma once
#include "core/threads.h"
#include "core/log.h"
#include "qvk/qvk.h"

#include <libavutil/pixfmt.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext_vulkan.h>

typedef struct decode_state_t
{
  AVStream       *av_stream;
  AVCodecContext *av_ctx;
  const AVCodec  *av_codec;
}
decode_state_t;

typedef struct decode_audio_ringbuffer_t
{
  AVFrame *frame[100]; // list of pointers to av frames allocated/freed by decode_video_receive_frame_audio
  int      rdi;        // next frame index to be read
  int      wri;        // next frame index to be received
  int      rpos;       // next reading position within frame
}
decode_audio_ringbuffer_t;

typedef struct decode_video_t
{
  int    wd;  // mirror of video.av_ctx->width
  int    ht;
  double fps; // mirror of av_q2d(v->video.av_stream->avg_frame_rate)

  AVFormatContext *av_format_ctx;
  AVPacket        *av_pkt;
  decode_state_t   video;
  decode_state_t   audio;

  int      channels;
  int      format;
  int      sample_rate;
  uint8_t *audio_buf;
  int      audio_size;
  int      audio_stride;

  int flushing;            // we're flushing the av codecs, don't send more packets

  int decode_tid; // task id for the decoder thread (av send packet), if not stopped
  threads_mutex_t av_mutex; // sync calls to av libs

  decode_audio_ringbuffer_t  arb;

  const AVCodecHWConfig   *hw_config;
  AVBufferRef             *hw_device;
  AVBufferRef             *frame_ctx;

  VkCommandBuffer          cmd;
  VkCommandPool            cmd_pool;
  VkSamplerYcbcrConversion ycbcr_conversion;
  VkSampler                sampler;
  VkDescriptorSetLayout    dset_layout;
  VkPipelineLayout         pipeline_layout;
  VkPipeline               pipeline;
  VkImageView              view; // XXX need more than one for pipelining?
}
decode_video_t;

typedef struct dt_graph_t dt_graph_t;
typedef struct dt_node_t dt_node_t;

// open video stream from filename (absolute resolved filename)
int decode_video_init(decode_video_t *v, dt_graph_t *graph, const char *filename);

// cleanup and free all resources
void decode_video_cleanup(decode_video_t *v);

// find all video decoding nodes, grab vk frames from avcodec,
// wrap image in an image view, dispatch conversion shader of video module (init its output connector)
// initialise extra timeline semaphore on graph to wait on.
// return 0 on success, a new frame has been served
int  decode_video_graph_run_pre_node(decode_video_t *v, dt_graph_t *graph, dt_node_t *node);
void decode_video_stop(decode_video_t *v);
void decode_seek(decode_video_t *v, double ts);

// copied from qvk_util.c so the module interface doesn't need it:
static inline const char *
qvk_result_to_string(VkResult result)
{
  switch(result) {
  case VK_SUCCESS: return "VK_SUCCESS";
  case VK_NOT_READY: return "VK_NOT_READY";
  case VK_TIMEOUT: return "VK_TIMEOUT";
  case VK_EVENT_SET: return "VK_EVENT_SET";
  case VK_EVENT_RESET: return "VK_EVENT_RESET";
  case VK_INCOMPLETE: return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
  case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
  case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
  case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
  case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
  case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
  case VK_ERROR_FRAGMENTATION_EXT: return "VK_ERROR_FRAGMENTATION_EXT";
  case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
  case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT: return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
  case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
  case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
  default: return "AAARRGHH";
  };
}

// copy of the one in graph.c so we don't have to expose it
static inline void *
read_file(const char *filename, size_t *len)
{
  FILE *f = dt_graph_open_resource(0, 0, filename, "rb");
  if(!f) return 0;
  fseek(f, 0, SEEK_END);
  const size_t filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *file = malloc(filesize+1);

  size_t rd = fread(file, sizeof(char), filesize, f);
  file[filesize] = 0;
  if(rd != filesize)
  {
    free(file);
    file = 0;
    fclose(f);
    return 0;
  }
  if(len) *len = filesize;
  fclose(f);
  return file;
}

static inline VkResult
dt_graph_create_shader_module(
    dt_graph_t     *graph,
    dt_token_t      node,
    dt_token_t      kernel,
    const char     *type,
    VkShaderModule *shader_module)
{
  // create the compute shader stage
  char filename[PATH_MAX+100] = {0};
  snprintf(filename, sizeof(filename), "modules/%"PRItkn"/%"PRItkn".%s.spv",
      dt_token_str(node), dt_token_str(kernel), type);

  size_t len;
  void *data = read_file(filename, &len);
  if(!data) return VK_ERROR_INVALID_EXTERNAL_HANDLE;

  VkShaderModuleCreateInfo sm_info = {
    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode    = data
  };
  QVKR(vkCreateShaderModule(qvk.device, &sm_info, 0, shader_module));
  free(data);
  return VK_SUCCESS;
}
