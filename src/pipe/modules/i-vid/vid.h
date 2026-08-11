#pragma once
#include "core/threads.h"
#include "core/log.h"

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
  // TODO flags like eof?
  // TODO timestamps?
}
decode_state_t;

typedef struct decode_video_t
{
  int    wd;  // mirror of video.av_ctx->width
  int    ht;
  double fps; // mirror of av_q2d(v->video.av_stream->avg_frame_rate)

  AVFormatContext *av_format_ctx;
  AVPacket        *av_pkt;
  decode_state_t   video;
  // decode_state_t   audio; // TODO

  int flushing;            // we're flushing the av codecs, don't send more packets

  int decode_tid; // task id for the decoder thread (av send packet), if not stopped
  threads_mutex_t av_mutex; // sync calls to av libs

  const AVCodecHWConfig   *hw_config;
  AVBufferRef             *hw_device;
  AVBufferRef             *frame_ctx;
  const AVCodec           *cached_av_codec;

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
