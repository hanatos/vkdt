#include "vid.h"
#include "qvk/qvk.h"
#include "pipe/graph.h"

static enum AVPixelFormat
decode_get_pixel_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
  decode_video_t *v = ctx->opaque;
  while (*pix_fmts != AV_PIX_FMT_NONE)
  {
    if (*pix_fmts == v->hw_config->pix_fmt)
    {
      if (avcodec_get_hw_frames_parameters(
            ctx, ctx->hw_device_ctx,
            v->hw_config->pix_fmt, &ctx->hw_frames_ctx) < 0)
      {
        dt_log(s_log_pipe|s_log_err, "avcodec_get_hw_frames_parameters failed!");
        return AV_PIX_FMT_NONE;
      }

      AVHWFramesContext *frames = (AVHWFramesContext *)ctx->hw_frames_ctx->data;
      AVVulkanFramesContext *vk = (AVVulkanFramesContext *)frames->hwctx;
      vk->img_flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

      if (av_hwframe_ctx_init(ctx->hw_frames_ctx) < 0)
      {
        dt_log(s_log_pipe|s_log_err, "av_hwframe_ctx_init failed!");
        av_buffer_unref(&ctx->hw_frames_ctx);
        return AV_PIX_FMT_NONE;
      }
      return *pix_fmts;
    }
    pix_fmts++;
  }

  return AV_PIX_FMT_NONE;
}

// init ffmpeg vulkan hardware decoder, or fail
int decode_video_init(decode_video_t *v, dt_graph_t *graph, const char *filename)
{
  memset(v, 0, sizeof(*v));
  v->decode_tid = -1; // no decode task started

  threads_mutex_init(&v->av_mutex, 0);

  int ret;
  if ((ret = avformat_open_input(&v->av_format_ctx, filename, 0, 0)) < 0)
  {
    dt_log(s_log_pipe|s_log_err, "avformat_open_input failed on %s");
    return 1;
  }

  if ((ret = av_find_best_stream(v->av_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, 0, 0)) < 0)
  {
    dt_log(s_log_pipe|s_log_err, "av_find_best_stream failed");
    return 1;
  }

  v->video.av_stream = v->av_format_ctx->streams[ret];
  const AVCodec *codec = avcodec_find_decoder(v->video.av_stream->codecpar->codec_id);

  dt_log(s_log_pipe, "avcodec: %s", codec->name);
  v->video.av_codec = codec;
  v->video.av_ctx = avcodec_alloc_context3(codec);
  if (!v->video.av_ctx)
  {
    dt_log(s_log_pipe|s_log_err, "avcodec_alloc_context3 failed!");
    return 1;
  }

  if (avcodec_parameters_to_context(v->video.av_ctx, v->video.av_stream->codecpar) < 0)
  {
    dt_log(s_log_pipe|s_log_err, "avcodec_parameters_to_context failed!");
    return 1;
  }
  v->video.av_ctx->opaque = v;

  if (!(v->av_pkt = av_packet_alloc()))
  {
    dt_log(s_log_pipe|s_log_err, "av_packet_alloc failed!");
    return 1;
  }

  for (int i = 0; !v->hw_device; i++)
  {
    const AVCodecHWConfig *config = avcodec_get_hw_config(v->video.av_codec, i); // return 0 once i becomes too large
    if (!config) break;
    if (config->device_type == AV_HWDEVICE_TYPE_NONE) continue;

    const char *hwdevice_name = av_hwdevice_get_type_name(config->device_type);
    dt_log(s_log_pipe, "found hw device type: %s", hwdevice_name);

    if (config->device_type != AV_HWDEVICE_TYPE_VULKAN) continue;
    if ((config->methods & (AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX | AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)) == 0) continue;

    AVBufferRef *hw_dev       = av_hwdevice_ctx_alloc(config->device_type);
    AVHWDeviceContext *hwctx  = (AVHWDeviceContext *)hw_dev->data;
    AVVulkanDeviceContext *vk = (AVVulkanDeviceContext *)hwctx->hwctx;

    hwctx->user_opaque = v;

    QVK_LOAD(vkGetInstanceProcAddr);
    vk->get_proc_addr = qvkGetInstanceProcAddr;
    vk->inst = qvk.instance;
    vk->act_dev = qvk.device;
    vk->phys_dev = qvk.physical_device;

    vk->device_features            = qvk.device_features;
    vk->enabled_inst_extensions    = qvk.inst_extension;
    vk->nb_enabled_inst_extensions = qvk.inst_extension_cnt;
    vk->enabled_dev_extensions     = qvk.dev_extension;
    vk->nb_enabled_dev_extensions  = qvk.dev_extension_cnt;

    // XXX does ffmpeg profit from any more?
    vk->nb_qf = 2;
    vk->qf[0] = (AVVulkanDeviceQueueFamily){
      .idx = qvk.queue_family_graphics,
      .num = 1,//qvk.queue[qvk.qid[s_queue_graphics]].num, // XXX seems wrong here
      .flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
    };
    vk->qf[1] = (AVVulkanDeviceQueueFamily){
      .idx = qvk.queue_family_vid_dec,
      .num = 1,//qvk.queue[qvk.qid[s_queue_vid_dec]].num,
      .flags = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
      .video_caps = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR,
    };

    // XXX use VK_KHR_internally_synchronized_queues for the decoder queue? we only have the one thread

    if (av_hwdevice_ctx_init(hw_dev) >= 0)
    {
      dt_log(s_log_pipe, "vk decoder hardware initialised.");
      v->hw_config = config;
      v->hw_device = hw_dev;
    }
    else av_buffer_unref(&hw_dev);
  }

  if (!v->hw_config)
  {
    dt_log(s_log_pipe|s_log_err, "failed to init vk hardware decoder!");
    return 1;
  }

  // XXX callback needed?
  v->video.av_ctx->get_format    = decode_get_pixel_format;
  v->video.av_ctx->hw_device_ctx = av_buffer_ref(v->hw_device);

  if (avcodec_open2(v->video.av_ctx, v->video.av_codec, 0) < 0)
  {
    dt_log(s_log_pipe|s_log_err, "avcodec_open2 failed!");
    return 1;
  }
  v->fps = av_q2d(v->video.av_stream->avg_frame_rate);
  if (v->fps == 0.0) v->fps = 24;

  v->wd = v->video.av_ctx->width;
  v->ht = v->video.av_ctx->height;
  dt_log(s_log_pipe, "vk decoder opened %s %d x %d @ %g", filename,
      v->wd, v->ht, v->fps);

  VkCommandPoolCreateInfo cmd_pool_create_info = {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = qvk.queue[qvk.qid[graph->queue_name]].family,
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };
  QVK(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, &v->cmd_pool));

  VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = v->cmd_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  QVK(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, &v->cmd));

  return 0;
}

void decode_video_cleanup(decode_video_t *v)
{
  if(!v) return;
  decode_video_stop(v); // needs mutex?
  // TODO make sure to wait on all semaphores before cleanup?
  vkDestroyImageView             (qvk.device, v->view, 0);
  vkFreeCommandBuffers           (qvk.device, v->cmd_pool, 1, &v->cmd);
  vkDestroyCommandPool           (qvk.device, v->cmd_pool, 0);
  vkDestroySampler               (qvk.device, v->sampler, 0);
  vkDestroySamplerYcbcrConversion(qvk.device, v->ycbcr_conversion, 0);
  vkDestroyPipelineLayout        (qvk.device, v->pipeline_layout, 0);
  vkDestroyPipeline              (qvk.device, v->pipeline, 0);
  vkDestroyDescriptorSetLayout   (qvk.device, v->dset_layout, 0);

  threads_mutex_destroy(&v->av_mutex);
  if (v->frame_ctx) av_buffer_unref(&v->frame_ctx);
  if (v->hw_device) av_buffer_unref(&v->hw_device);

  avcodec_free_context(&v->video.av_ctx);
  // avcodec_free_context(&v->audio->av_ctx);

  if (v->av_format_ctx)
    avformat_close_input(&v->av_format_ctx);
  if (v->av_pkt)
    av_packet_free(&v->av_pkt);
  memset(v, 0, sizeof(*v));
}
