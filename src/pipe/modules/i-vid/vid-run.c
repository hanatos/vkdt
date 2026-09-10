#include "vid.h"
#include "pipe/node.h"
#include "pipe/modules/api.h"

// TODO worker thread for upload/decode! (do we have an issue with speed?)

static VkResult
decode_video_create_sampler(
    decode_video_t *v,
    VkFormat        format, // for instance we see VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
    dt_graph_t     *graph,
    dt_node_t      *node)
{
  if(v->sampler) return VK_SUCCESS; // assume that's already constructed
  VkFormatProperties2 fp = { .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
  vkGetPhysicalDeviceFormatProperties2(qvk.physical_device, format, &fp);
  int cosited = (fp.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) != 0;
  VkSamplerYcbcrConversionCreateInfo conversion_info = {
    .sType         = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
    .format        = format,
    // XXX these next two can depend on user settings!
    .ycbcrModel    = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
    .ycbcrRange    = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
    .xChromaOffset = cosited ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT,
    .yChromaOffset = cosited ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT,
    .chromaFilter  = VK_FILTER_LINEAR,
    .forceExplicitReconstruction = VK_FALSE,
  };
  QVKR(vkCreateSamplerYcbcrConversion(qvk.device, &conversion_info, 0, &v->ycbcr_conversion));
  VkSamplerYcbcrConversionInfo ycbcr_info = {
    .sType      = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
    .conversion = v->ycbcr_conversion,
  };
  VkSamplerCreateInfo sampler_yuv_info = {
    .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext        = &ycbcr_info,
    .magFilter    = VK_FILTER_LINEAR,
    .minFilter    = VK_FILTER_LINEAR,
    .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .minLod       = 0.0f,
    .maxLod       = 1.0f,
    .borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };
  QVKR(vkCreateSampler(qvk.device, &sampler_yuv_info, NULL, &v->sampler));

  VkDescriptorSetLayoutBinding bindings[] = {{
    .binding            = 0,
    .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount    = 1,
    .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
    .pImmutableSamplers = &v->sampler,
  },{
    .binding            = 1,
    .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    .descriptorCount    = 1,
    .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
  }};
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT|VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 2,
    .pBindings    = bindings,
  };
  QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &v->dset_layout));
  // create our own pipeline layout
  VkDescriptorSetLayout dset_layout[] = {
    graph->uniform_dset_layout,
    v->dset_layout,
  };
  VkPushConstantRange pcrange = {
    .stageFlags = VK_SHADER_STAGE_ALL,
    .offset     = 0,
    .size       = node->push_constant_size,
  };
  VkPipelineLayoutCreateInfo layout_info = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount         = LENGTH(dset_layout),
    .pSetLayouts            = dset_layout,
    .pushConstantRangeCount = node->push_constant_size ? 1 : 0,
    .pPushConstantRanges    = node->push_constant_size ? &pcrange : 0,
  };
  QVKR(vkCreatePipelineLayout(qvk.device, &layout_info, 0, &v->pipeline_layout));

  VkShaderModule shader_module;
  QVKR(dt_graph_create_shader_module(graph, node->name, node->kernel, "comp", &shader_module));
  VkPipelineShaderStageCreateInfo stage_info = {
    .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
    .pSpecializationInfo = 0,
    .pName               = "main",
    .module              = shader_module,
  };
  VkComputePipelineCreateInfo pipeline_info = {
    .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage  = stage_info,
    .layout = v->pipeline_layout,
  };
  QVKR(vkCreateComputePipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &v->pipeline));
  vkDestroyShaderModule(qvk.device, stage_info.module, 0);
  return VK_SUCCESS;
}

// bg thread decoder: send packets
static int // 0 ok, 1 sleep, 2 error, 3 eof
decode_video_send_packets(decode_video_t *v)
{
  if(v->flushing) return 3; // nothing to do for us
  threads_mutex_lock(&v->av_mutex);
  int result = 0;

  int res;
  res = av_read_frame(v->av_format_ctx, v->av_pkt);
  if(res < 0)
  { // eof or error
    // XXX do nothing?
    result = 2;
  }
  else
  {
    if(v->av_pkt->stream_index == v->video.av_stream->index)
    { // video packet
      int ret = avcodec_send_packet(v->video.av_ctx, v->av_pkt);
      // if(ret == 0) // success
      if(ret == AVERROR(EAGAIN)) result = 1; // go to sleep, must call avcodec_receive_frame before we can go on
      else if(ret == AVERROR_EOF)
      { // flushed, can't send any more packets
        avcodec_send_packet(v->video.av_ctx, 0); // flush
        v->flushing = 1;
        return 3;
      }
      else if(ret == AVERROR(EINVAL)) result = 2; // really broken
    }
    else if(v->audio.av_ctx && v->av_pkt->stream_index == v->audio.av_stream->index)
    { // audio packet
      int ret = avcodec_send_packet(v->audio.av_ctx, v->av_pkt);
      // if(ret == 0) // success
      if(ret == AVERROR(EAGAIN)) result = 1; // go to sleep, must call avcodec_receive_frame before we can go on
      else if(ret == AVERROR_EOF)
      { // flushed, can't send any more packets
        avcodec_send_packet(v->audio.av_ctx, 0); // flush
        return 3;
      }
      else if(ret == AVERROR(EINVAL)) result = 2; // really broken
    }
    av_packet_unref(v->av_pkt);
  }

  threads_mutex_unlock(&v->av_mutex);
  return result;
}

static int // 0 ok, 1 need more packets first, 2 error, 3 eof
decode_video_receive_frame_audio(
    decode_video_t *v,
    dt_graph_t     *graph,
    dt_node_t      *node)
{
  if(!v->audio.av_ctx) return 0;
  int res = 0;
  threads_mutex_lock(&v->av_mutex);
  if(v->arb.frame[v->arb.wri]) av_frame_free(v->arb.frame + v->arb.wri);
  v->arb.frame[v->arb.wri] = av_frame_alloc();
  if (!v->arb.frame[v->arb.wri]) res = 2;
  else res = avcodec_receive_frame(v->audio.av_ctx, v->arb.frame[v->arb.wri]);
  if(res == AVERROR(EAGAIN)) res = 1;
  if(res == AVERROR(EINVAL)) res = 2;
  if(res == AVERROR_EOF)     res = 3;
  
  if(!res) v->arb.wri = (v->arb.wri+1)%LENGTH(v->arb.frame);

  threads_mutex_unlock(&v->av_mutex);
  return res;
}

// XXX need av mutex locked?
static VkResult
decode_video_copy_img_cmd(
    decode_video_t *v,
    dt_graph_t     *graph,
    dt_node_t      *node,
    AVFrame        *frame)
{
  AVHWFramesContext *frames = (AVHWFramesContext *)v->video.av_ctx->hw_frames_ctx->data;
  AVVulkanFramesContext *vk = (AVVulkanFramesContext *)frames->hwctx;
  AVVkFrame       *vk_frame = (AVVkFrame *)frame->data[0];
  vk->lock_frame(frames, vk_frame);

  // have to re-create these because ffmpeg will hand us different vkimages over time
  VkSamplerYcbcrConversionInfo ycbcr_info = {
    .sType      = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
    .conversion = v->ycbcr_conversion,
  };
  if(v->view) vkDestroyImageView(qvk.device, v->view, 0);
  v->view = 0;
  VkImageViewCreateInfo info = {
    .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext    = &ycbcr_info,
    .image    = vk_frame->img[0],
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format   = vk->format[0],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, // we'll be using sampler ycbcr conversion
      .levelCount = 1,
      .layerCount = 1,
    },
  };
  // XXX we actually don't want this to be a storage image, maybe ffmpeg depends on this?
  // [qvk] validation layer: vkCreateImageView(): pCreateInfo->format VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 with tiling VK_IMAGE_TILING_OPTIMAL doesn't support VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT.
  vkCreateImageView(qvk.device, &info, 0, &v->view);
  VkDescriptorImageInfo img_in = {
    .imageView   = v->view,
    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };
  dt_connector_image_t *img = dt_graph_connector_image(graph,
      node - graph->node, 0, 0, graph->double_buffer);
  assert(img->image_view);
  VkDescriptorImageInfo img_out = {
    .imageView   = img->image_view,
    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };
  VkWriteDescriptorSet write_dset[] = {{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding      = 0,
    .descriptorCount = 1,
    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo      = &img_in,
  },{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding      = 1,
    .descriptorCount = 1,
    .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    .pImageInfo      = &img_out,
  }};

  VkCommandBuffer cmd_buf = v->cmd;
  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  QVKR(vkBeginCommandBuffer(cmd_buf, &begin_info));
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, v->pipeline);
  QVK_LOAD(vkCmdPushDescriptorSetKHR);
  qvkCmdPushDescriptorSetKHR(
      cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
      v->pipeline_layout, 1, 2, write_dset);

  vkCmdDispatch(cmd_buf,
      (v->wd + DT_LOCAL_SIZE_X - 1) / DT_LOCAL_SIZE_X,
      (v->ht + DT_LOCAL_SIZE_Y - 1) / DT_LOCAL_SIZE_Y,
       1);

  VkPipelineStageFlags wait_stage[] = {
    VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT };
  uint64_t    val_wait  [] = { vk_frame->sem_value[0] };
  uint64_t    val_signal[] = { ++vk_frame->sem_value[0], ++graph->semaphore_extra_val };
  VkSemaphore sem_wait  [] = { vk_frame->sem[0] };
  VkSemaphore sem_signal[] = { vk_frame->sem[0], graph->semaphore_extra };
  VkTimelineSemaphoreSubmitInfo timeline_info = {
    .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount   = 1,
    .pWaitSemaphoreValues      = val_wait,
    .signalSemaphoreValueCount = 2,
    .pSignalSemaphoreValues    = val_signal,
  };
  VkSubmitInfo submit = {
    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext                = &timeline_info,
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = sem_wait,
    .pWaitDstStageMask    = wait_stage,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &cmd_buf,
    .signalSemaphoreCount = 2,
    .pSignalSemaphores    = sem_signal,
  };
  vk->unlock_frame(frames, vk_frame);

  QVKR(vkEndCommandBuffer(cmd_buf));
  qvk_queue_name_t submit_queue = graph->use_graphics_queue ? s_queue_graphics : graph->queue_name;
  QVKLR(&qvk.queue[qvk.qid[submit_queue]].mutex,
      vkQueueSubmit(qvk.queue[qvk.qid[submit_queue]].queue, 1, &submit, 0));
  return VK_SUCCESS;
}

static int // 0 ok, 1 need more packets first, 2 error, 3 eof
decode_video_receive_frame(
    decode_video_t *v,
    dt_graph_t     *graph,
    dt_node_t      *node)
{
  int res = 0;
  threads_mutex_lock(&v->av_mutex);

  AVFrame *frame = av_frame_alloc();
  if (!frame) { res = 2; goto out; }

  // need to call send_packet and receive_frame from same thread, apparently.
  res = avcodec_receive_frame(v->video.av_ctx, frame);
  if(res == AVERROR(EAGAIN)) { res = 1; goto out; }
  if(res == AVERROR(EINVAL)) { res = 2; goto out; }
  if(res == AVERROR_EOF)     { res = 3; goto out; }
  assert(res == 0);

#if 0 // make sure pts_img is not lagging behind
  // generally we receive these frames in order from ffmpeg/vkdec
  double pts_our = graph->frame / v->fps;
  double time_base = av_q2d(v->video.av_stream->time_base);
  double pts_img = frame->pts * time_base;
  if(pts_our > pts_img + time_base) { res = 1; goto out; }
  fprintf(stderr, "timestamps %g %g\n", pts_our, pts_img); // perfect match
#endif

  AVHWFramesContext *frames = (AVHWFramesContext *)v->video.av_ctx->hw_frames_ctx->data;
  AVVulkanFramesContext *vk = (AVVulkanFramesContext *)frames->hwctx;
  decode_video_create_sampler(v, vk->format[0], graph, node); // create ycbcr conversion if we don't have it yet
  decode_video_copy_img_cmd(v, graph, node, frame);
out:
  av_frame_free(&frame);
  threads_mutex_unlock(&v->av_mutex);
  return res;
}

int // return 0 on success, a new frame has been served
decode_video_graph_run_pre_node(
    decode_video_t *v,
    dt_graph_t     *graph,
    dt_node_t      *node)
{
again:;
#if 1
  { // XXX DEBUG run inline instead of in decoder thread:
    int res = 0;
    res = decode_video_send_packets(v);
    // res: 0 ok, 1 sleep, 2 error, 3 eof
    if(res == 2) return 1; // can't recover
  }
#endif
  int res = 0;
  res = decode_video_receive_frame_audio(v, graph, node);
  res = decode_video_receive_frame(v, graph, node);
  if(res == 1)
  { // need to wait for decoder (thread) to finalise a new frame for us!
    // fprintf(stderr, "XXX requesting more packets! frame %d\n", graph->frame); // happens in bulk, causes stutter
    goto again;
  }
  if(res == 2) return 1; // error
  if(res == 3) return 1; // eof, last frame served already
  return 0;
}

void
decode_video_stop(decode_video_t *v)
{
  // TODO threading stuff:
  // if(v->decode_tid < 0) return; // XXX race condition?
  // threads_wait(v->decode_tid);
  // v->decode_tid = -1; // should have been done in decode_task_done already, but hey.
  avcodec_flush_buffers(v->video.av_ctx);
  v->arb.rdi = v->arb.wri = v->arb.rpos = 0;
}

void
decode_seek(decode_video_t *v, double ts)
{
  if (!v->av_format_ctx) return;
  if (ts < 0.0) ts = 0.0;
  int64_t target_ts = (int64_t)(AV_TIME_BASE * ts);
  if (avformat_seek_file(v->av_format_ctx, -1, INT64_MIN, target_ts, INT64_MAX, 0) < 0)
  {
    dt_log(s_log_pipe|s_log_err, "failed to seek video");
    return;
  }
  avcodec_flush_buffers(v->video.av_ctx);
  v->arb.rdi = v->arb.wri = v->arb.rpos = 0;
}

#if 0
static void
decode_task_done(void *data)
{
  decode_video_t *v = data;
  v->decode_tid = -1;
}

// TODO background thread
static void
decode_task_work(uint32_t item, void *data)
{
  // TODO send packet
}

static inline void
decode_video_play(decode_video_t *v)
{
  v->teardown = 0;
  // TODO start audio too
  v->decode_tid = threads_task("decode", 1, -1, v, decode_task_work, decode_task_done);
}
#endif
