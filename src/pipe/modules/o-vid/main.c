#include "modules/api.h"
#include "core/core.h"
#include "core/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

typedef struct output_stream_t
{ // a wrapper around a single output AVStream
  AVStream *st;
  AVCodecContext *enc;

  /* pts of the next frame that will be generated */
  int64_t next_pts;
  uint64_t sample_pos; // used to compute time stamp for audio and query the audio module
  uint64_t sample_cnt; // how many samples written to output

  AVFrame *frame;
  AVFrame *tmp_frame;

  AVPacket *tmp_pkt;
  struct SwrContext *swr_ctx;
}
output_stream_t;

typedef struct buf_t
{
  AVFormatContext *oc;
  const AVCodec  *audio_codec, *video_codec;
  output_stream_t audio_stream, video_stream;
  int audio_mod;   // the module on our graph that has the audio
  int have_buf[3]; // flag that we have read the buffers for Y Cb Cr for the given frame
  double time_beg;
}
buf_t;

static inline void
close_stream(AVFormatContext *oc, output_stream_t *ost)
{
  avcodec_free_context(&ost->enc);
  av_frame_free(&ost->frame);
  av_frame_free(&ost->tmp_frame);
  av_packet_free(&ost->tmp_pkt);
  swr_free(&ost->swr_ctx);
}

static inline void
close_file(buf_t *dat)
{
  if(!dat->oc) return;
  av_write_trailer(dat->oc);

  close_stream(dat->oc, &dat->video_stream);
  close_stream(dat->oc, &dat->audio_stream);

  // if (!(fmt->flags & AVFMT_NOFILE))
  avio_closep(&dat->oc->pb);

  double end = dt_time();
  fprintf(stderr, "[o-vid] wrote video in %.3f seconds.\n", (end - dat->time_beg));

  avformat_free_context(dat->oc);
  dat->oc = 0;
}

static inline int
add_stream(
    dt_module_t     *mod,
    output_stream_t *ost,
    AVFormatContext *oc,
    const AVCodec  **codec,
    enum AVCodecID   codec_id)
{
  AVCodecContext *c;
  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec))
  {
    fprintf(stderr, "[o-vid] could not find encoder for '%s'\n", avcodec_get_name(codec_id));
    return 1;
  }

  ost->tmp_pkt = av_packet_alloc();
  if (!ost->tmp_pkt)
  {
    fprintf(stderr, "[o-vid] could not allocate AVPacket\n");
    return 1;
  }

  ost->st = avformat_new_stream(oc, NULL);
  if (!ost->st)
  {
    fprintf(stderr, "[o-vid] could not allocate stream\n");
    return 1;
  }
  ost->st->id = oc->nb_streams-1;
  c = avcodec_alloc_context3(*codec);
  if (!c)
  {
    fprintf(stderr, "[o-vid] could not alloc an encoding context\n");
    return 1;
  }
  ost->enc = c;

  const float frame_rate = mod->graph->frame_rate > 0.0f ? mod->graph->frame_rate : 24;

  switch ((*codec)->type)
  {
    case AVMEDIA_TYPE_AUDIO:
#if 0 // ffmpeg7:
      AVSampleFormat *sample_fmts;
      int ret = avcodec_get_supported_config(
          c,
          codec,
          AV_CODEC_CONFIG_SAMPLE_FORMAT,
          0,
          &sample_fmts,
          0);
      c->sample_fmt = sample_fmts[0] != AV_SAMPLE_FMT_NONE ? sample_fmts[0] : AV_SAMPLE_FMT_S16;
#else
      // direct access now deprecated in ffmpeg 7
      c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_S16;
#endif
      // c->sample_fmt  = AV_SAMPLE_FMT_S16;
      c->bit_rate    = 64000;
      c->sample_rate = 48000;
      // TODO: see dance above, get zero terminated list of supported samplerates, int
      if ((*codec)->supported_samplerates)
      {
        c->sample_rate = (*codec)->supported_samplerates[0];
        for (int i = 0; (*codec)->supported_samplerates[i]; i++)
        {
          if ((*codec)->supported_samplerates[i] == 48000) c->sample_rate = 48000;
        }
      }
      // TODO: ch_layout also has a callback, AVChannelLayout, zero terminated
      av_channel_layout_copy(&c->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO);
      ost->st->time_base = (AVRational){ 1, c->sample_rate };
      break;

    case AVMEDIA_TYPE_VIDEO:
      c->codec_id = codec_id;
      /* timebase: This is the fundamental unit of time (in seconds) in terms
       * of which frame timestamps are represented. For fixed-fps content,
       * timebase should be 1/framerate and timestamp increments should be
       * identical to 1. */
      ost->st->time_base = (AVRational){ 1, frame_rate };
      c->time_base       = ost->st->time_base;

      // c->bit_rate = 400000;
      /* Resolution must be a multiple of two. */
      c->width    = mod->connector[0].roi.wd & ~1;
      c->height   = mod->connector[0].roi.ht & ~1;
      // c->gop_size = 12; /* emit one intra frame every twelve frames at most */
      c->color_range = AVCOL_RANGE_MPEG; // limited (prores uses the other bits)
      switch(mod->img_param.colour_primaries)
      {
        case s_colour_primaries_srgb:
          c->color_primaries = AVCOL_PRI_BT709;
          c->colorspace      = AVCOL_SPC_BT709;
          break;
        case s_colour_primaries_2020:
          c->color_primaries = AVCOL_PRI_BT2020;
          c->colorspace      = AVCOL_SPC_BT2020_NCL;
          break;
        case s_colour_primaries_P3  : c->color_primaries = AVCOL_PRI_SMPTE432; break;
        default: c->color_primaries = AVCOL_PRI_UNSPECIFIED;
      }
      switch(mod->img_param.colour_trc)
      {
        case s_colour_trc_linear: c->color_trc = AVCOL_TRC_LINEAR;       break;
        case s_colour_trc_709:    c->color_trc = AVCOL_TRC_BT709;        break;
        case s_colour_trc_PQ:     c->color_trc = AVCOL_TRC_SMPTE2084;    break;
        case s_colour_trc_DCI:    c->color_trc = AVCOL_TRC_SMPTE428;     break;
        case s_colour_trc_HLG:    c->color_trc = AVCOL_TRC_ARIB_STD_B67; break;
        case s_colour_trc_gamma:  c->color_trc = AVCOL_TRC_GAMMA22;      break; // adobe rgb gamma
        default: c->color_trc = AVCOL_TRC_UNSPECIFIED;
      }
      c->chroma_sample_location = AVCHROMA_LOC_CENTER; // do we care? do i understand what this means?

      if(c->codec_id == AV_CODEC_ID_H264)
      {
        c->pix_fmt = AV_PIX_FMT_YUV420P;
        /// Compression efficiency (slower -> better quality + higher cpu%)
        /// [ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow]
        /// Set this option to "ultrafast" is critical for realtime encoding
        av_opt_set(c->priv_data, "preset", "ultrafast", 0);

        /// Compression rate (lower -> higher compression) compress to lower size, makes decoded image more noisy
        /// Range: [0; 51], sane range: [18; 26]. I used 35 as good compression/quality compromise. This option also critical for realtime encoding
        const float p_quality = dt_module_param_float(mod, 1)[0];
        int crf = 35 - p_quality/100.0 * (35-16);
        char str[5] = {0};
        snprintf(str, sizeof(str), "%d", crf);
        av_opt_set(c->priv_data, "crf", str, 0);

        /// Change settings based upon the specifics of input
        /// [psnr, ssim, grain, zerolatency, fastdecode, animation]
        /// This option is most critical for realtime encoding, because it removes delay between 1th input frame and 1th output packet.
        // i think this is most crucial for realtime *decoding* because i don't care so much about latency. it does slow us down.
        // av_opt_set(c->priv_data, "tune", "zerolatency", 0);
      }
      else if(c->codec_id == AV_CODEC_ID_PRORES)
      {
        const int p_profile = CLAMP(dt_module_param_int(mod, 3)[0], 0, 3);
        c->profile = p_profile;
        c->pix_fmt = AV_PIX_FMT_YUV422P10LE;
        // av_dict_set(&opt, "vendor", "apl0", 0);//???
#if 0
//TODO: support these:
avcodec.h:1719:#define FF_PROFILE_PRORES_4444      4
avcodec.h:1720:#define FF_PROFILE_PRORES_XQ
#endif
      }
      break;

    default:
      break;
  }

  /* Some formats want stream headers to be separate. */
  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  return 0;
}

static inline AVFrame*
alloc_frame(enum AVPixelFormat pix_fmt, int width, int height)
{
  AVFrame *frame = av_frame_alloc();
  if (!frame) return 0;

  frame->format = pix_fmt;
  frame->width  = width;
  frame->height = height;

  /* allocate the buffers for the frame data */
  if(av_frame_get_buffer(frame, 0) < 0)
  {
    fprintf(stderr, "[o-vid] could not allocate frame data.\n");
    return 0;
  }
  return frame;
}

static inline void
open_video(
    dt_module_t *mod,
    AVFormatContext *oc, const AVCodec *codec,
    output_stream_t *ost, AVDictionary *opt_arg)
{
  AVCodecContext *c = ost->enc;
  AVDictionary *opt = NULL;
  // have not been very successful in making this faster:
  c->thread_count = 16;
  // c->thread_type = FF_THREAD_SLICE;

  av_dict_copy(&opt, opt_arg, 0);

  /* open the codec */
  int ret = avcodec_open2(c, codec, &opt);
  av_dict_free(&opt);
  if (ret < 0)
  {
    fprintf(stderr, "[o-vid] could not open video codec: %s\n", av_err2str(ret));
    return;
  }

  /* allocate and init a re-usable frame */
  ost->frame = alloc_frame(c->pix_fmt, c->width, c->height);
  if (!ost->frame)
  {
    fprintf(stderr, "[o-vid] could not allocate video frame\n");
    return;
  }

  /* copy the stream parameters to the muxer */
  if(avcodec_parameters_from_context(ost->st->codecpar, c) < 0)
  {
    fprintf(stderr, "[o-vid] could not copy the stream parameters\n");
    return;
  }
}

static inline AVFrame*
alloc_audio_frame(
    enum AVSampleFormat sample_fmt,
    const AVChannelLayout *channel_layout,
    int sample_rate, int nb_samples)
{
  AVFrame *frame = av_frame_alloc();
  if (!frame)
  {
    fprintf(stderr, "[o-vid] error allocating an audio frame\n");
    return 0;
  }

  frame->format = sample_fmt;
  av_channel_layout_copy(&frame->ch_layout, channel_layout);
  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;

  if (nb_samples && av_frame_get_buffer(frame, 0) < 0)
  {
    fprintf(stderr, "[o-vid] error allocating an audio buffer\n");
    return 0;
  }
  return frame;
}

static inline void
open_audio(
    dt_module_t *mod,
    AVFormatContext *oc, const AVCodec *codec,
    output_stream_t *ost, AVDictionary *opt_arg)
{
  AVCodecContext *c;
  int nb_samples;
  int ret;
  AVDictionary *opt = NULL;

  c = ost->enc;

  av_dict_copy(&opt, opt_arg, 0);
  ret = avcodec_open2(c, codec, &opt);
  av_dict_free(&opt);
  if (ret < 0)
  {
    fprintf(stderr, "[o-vid] could not open audio codec: %s\n", av_err2str(ret));
    return;
  }

  nb_samples = c->frame_size;

  ost->frame = alloc_audio_frame(c->sample_fmt, &c->ch_layout, c->sample_rate, nb_samples);
  // this is the frame as it comes from our audio module: we need to init it according to audio_mod img_param sound properties:
  buf_t *dat = mod->data;
  int src_fmt = mod->graph->module[dat->audio_mod].img_param.snd_format == 2 ? // TODO: support others
      AV_SAMPLE_FMT_S16 : 0;
  int src_sample_rate = mod->graph->module[dat->audio_mod].img_param.snd_samplerate;

  AVChannelLayout src_layout = mod->graph->module[dat->audio_mod].img_param.snd_channels == 2?
    (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO :
    (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
  ost->tmp_frame = alloc_audio_frame(src_fmt, &src_layout, src_sample_rate, nb_samples);

  /* copy the stream parameters to the muxer */
  if(avcodec_parameters_from_context(ost->st->codecpar, c) < 0)
  {
    fprintf(stderr, "[o-vid] could not copy the stream parameters\n");
    return;
  }

  ost->swr_ctx = 0;
  if(av_channel_layout_compare(&src_layout, &c->ch_layout) || src_sample_rate != c->sample_rate || src_fmt != c->sample_fmt)
  {
    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx)
    {
      fprintf(stderr, "[o-vid] could not allocate resampler context\n");
      return;
    }

    /* set options */
    av_opt_set_chlayout  (ost->swr_ctx, "in_chlayout",       &src_layout,        0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     src_sample_rate,   0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      src_fmt,           0);
    av_opt_set_chlayout  (ost->swr_ctx, "out_chlayout",      &c->ch_layout,      0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0)
    {
      fprintf(stderr, "[o-vid] failed to initialise the resampling context\n");
      return;
    }
  }
}

static inline int
open_file(dt_module_t *mod)
{
  buf_t *dat = mod->data;
  if(dat->oc) close_file(dat);
  dat->time_beg = dt_time();
  dat->audio_mod = -1;
  for(int i=0;i<mod->graph->num_modules;i++)
    if(mod->graph->module[i].name && mod->graph->module[i].so->audio) { dat->audio_mod = i; break; }
  const char *basename  = dt_module_param_string(mod, 0);
  char filename[512];
  const int p_codec = dt_module_param_int(mod, 2)[0];
  if(p_codec == 0) snprintf(filename, sizeof(filename), "%s.mov", basename);
  else             snprintf(filename, sizeof(filename), "%s.mp4", basename);

  const int width  = mod->connector[0].roi.wd & ~1;
  const int height = mod->connector[0].roi.ht & ~1;
  if(width <= 0 || height <= 0) return 1;

  if(p_codec == 0) avformat_alloc_output_context2(&dat->oc, NULL, "mov", filename);
  else             avformat_alloc_output_context2(&dat->oc, NULL, "mp4", filename);
  if (!dat->oc) return 1;

  const AVOutputFormat *fmt = dat->oc->oformat;

  enum AVCodecID codec_id = AV_CODEC_ID_H264;
  if(p_codec == 0) codec_id = AV_CODEC_ID_PRORES;
  add_stream(mod, &dat->video_stream, dat->oc, &dat->video_codec, codec_id);  
  add_stream(mod, &dat->audio_stream, dat->oc, &dat->audio_codec, fmt->audio_codec);

  AVDictionary *opt = NULL;
  open_video(mod, dat->oc, dat->video_codec, &dat->video_stream, opt);
  open_audio(mod, dat->oc, dat->audio_codec, &dat->audio_stream, opt);

  int ret = avio_open(&dat->oc->pb, filename, AVIO_FLAG_WRITE);
  if (ret < 0)
  {
    fprintf(stderr, "[o-vid] could not open '%s': %s\n", filename, av_err2str(ret));
    return 1;
  }

  /* Write the stream header, if any. */
  ret = avformat_write_header(dat->oc, &opt);
  if (ret < 0)
  {
    fprintf(stderr, "[o-vid] error occurred when opening output file: %s\n", av_err2str(ret));
    return 1;
  }
  return 0;
}

static inline int
write_frame(
    AVFormatContext *fmt_ctx, AVCodecContext *c,
    AVStream *st, AVFrame *frame, AVPacket *pkt)
{
  // send the frame to the encoder
  int ret = avcodec_send_frame(c, frame);
  if (ret < 0)
  {
    fprintf(stderr, "[o-vid] error sending a frame to the encoder: %s\n", av_err2str(ret));
    return 1;
  }

  while (ret >= 0)
  {
    ret = avcodec_receive_packet(c, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else if (ret < 0)
    {
      fprintf(stderr, "[o-vid] error encoding a frame: %s\n", av_err2str(ret));
      return 1;
    }

    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, c->time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    ret = av_interleaved_write_frame(fmt_ctx, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0)
    {
      fprintf(stderr, "[o-vid] error while writing output packet: %s\n", av_err2str(ret));
      return 1;
    }
  }
  if(ret == AVERROR_EOF) return 1;
  return 0;
}

// =================================================
//  module api callbacks
// =================================================

int init(dt_module_t *mod)
{
  buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  mod->flags = s_module_request_write_sink;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  buf_t *dat = mod->data;
  close_file(dat);
  free(dat);
  mod->data = 0;
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{ // encode kernel, wire sink modules for Y Cb Cr
  const int wd = module->connector[0].roi.wd & ~1; // pad to multiple of two for ffmpeg
  const int ht = module->connector[0].roi.ht & ~1;
  dt_roi_t roi_Y  = { .wd = wd, .ht = ht };
  dt_roi_t roi_CbCr = roi_Y;
  const int p_codec = dt_module_param_int(module, 2)[0];
  int bits  = 0; // 8 bit
  int chr   = 0; // 420 chroma subsampling
  int range = 0; // mpeg/limited range
  if(p_codec == 0)
  { // prores 422
    roi_CbCr.wd /= 2;
    bits = 1; // 10 bit
    chr  = 1; // 422
  }
  else if(p_codec == 1)
  { // h264 420
    roi_CbCr.wd /= 2;
    roi_CbCr.ht /= 2;
  }
  int pc[] = { bits, chr, range };
  const int id_enc = dt_node_add(graph, module, "o-vid", "enc", wd, ht, 1, sizeof(pc), pc, 4,
      "input", "read", "rgba", "f16", dt_no_roi,
      "Y",     "write", "r",   bits ? "ui16" : "ui8", &roi_Y,
      "Cb",    "write", "r",   bits ? "ui16" : "ui8", &roi_CbCr,
      "Cr",    "write", "r",   bits ? "ui16" : "ui8", &roi_CbCr);
  const int id_Y  = dt_node_add(graph, module, "o-vid", "Y",  1, 1, 1, 0, 0, 1,
      "Y",  "sink", "r", bits ? "ui16" : "ui8", dt_no_roi);
  const int id_Cb = dt_node_add(graph, module, "o-vid", "Cb", 1, 1, 1, 0, 0, 1,
      "Cb", "sink", "r", bits ? "ui16" : "ui8", dt_no_roi);
  const int id_Cr = dt_node_add(graph, module, "o-vid", "Cr", 1, 1, 1, 0, 0, 1,
      "Cr", "sink", "r", bits ? "ui16" : "ui8", dt_no_roi);
  CONN(dt_node_connect_named(graph, id_enc, "Y",  id_Y,  "Y"));
  CONN(dt_node_connect_named(graph, id_enc, "Cb", id_Cb, "Cb"));
  CONN(dt_node_connect_named(graph, id_enc, "Cr", id_Cr, "Cr"));
  dt_connector_copy(graph, module, 0, id_enc, 0);
}

// TODO: check params that reconstruct nodes on codec/chroma sampling change!

// yuv422p10le  : y full res, cb and cr half res in x, full res in y
// yuva444p10le : y, cb, cr, alpha all fullres 10 bits padded to 16 bits
// yuv420p for h264, which is the same as in this example, 8bps for all channels
// yuv444p16le which we could use as input, but it only scales the data (it is padded to 16 in the case of 10bits)
void write_sink(
    dt_module_t            *mod,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  buf_t *dat = mod->data;
  if(mod->graph->frame < mod->graph->frame_cnt-1 && !dat->oc) open_file(mod);
  if(!dat->oc) return; // avoid crashes in case opening the file went wrong
  output_stream_t *vost = &dat->video_stream;

  const int wd = mod->connector[0].roi.wd & ~1;
  const int ht = mod->connector[0].roi.ht & ~1;

  /* when we pass a frame to the encoder, it may keep a reference to it
   * internally; make sure we do not overwrite it here */
  if (av_frame_make_writable(vost->frame) < 0) return;

  const int p_codec = dt_module_param_int(mod, 2)[0];
  uint8_t  *mapped8  = buf;
  uint16_t *mapped16 = buf;
  if(p->node->kernel == dt_token("Y"))
  {
    if(p_codec == 0)
      for(int j=0;j<ht;j++)
        memcpy(&vost->frame->data[0][j * vost->frame->linesize[0]], mapped16 + j*wd, sizeof(uint16_t)*wd);
    else
      for(int j=0;j<ht;j++)
        memcpy(&vost->frame->data[0][j * vost->frame->linesize[0]], mapped8 + j*wd, sizeof(uint8_t)*wd);
    dat->have_buf[0] = 1;
  }
  else if(p->node->kernel == dt_token("Cb"))
  { // ffmpeg expects the colour planes separate, not in a 2-channel texture
    if(p_codec == 0)
      for(int j=0;j<ht;j++)
        memcpy(&vost->frame->data[1][j * vost->frame->linesize[1]], mapped16 + j*(wd/2), sizeof(uint16_t)*wd/2);
    else
      for(int j=0;j<ht/2;j++)
        memcpy(&vost->frame->data[1][j * vost->frame->linesize[1]], mapped8 + j*(wd/2), sizeof(uint8_t)*wd/2);
    dat->have_buf[1] = 1;
  }
  else if(p->node->kernel == dt_token("Cr"))
  {
    if(p_codec == 0)
      for(int j=0;j<ht;j++)
        memcpy(&vost->frame->data[2][j * vost->frame->linesize[2]], mapped16 + j*(wd/2), sizeof(uint16_t)*wd/2);
    else
      for(int j=0;j<ht/2;j++)
        memcpy(&vost->frame->data[2][j * vost->frame->linesize[2]], mapped8 + j*(wd/2), sizeof(uint8_t)*wd/2);
    dat->have_buf[2] = 1;
  }
  // alpha has no connector and copies no data

  if(dat->have_buf[0] && dat->have_buf[1] && dat->have_buf[2])
  { // if we have all three channels for a certain frame:
    vost->frame->pts = vost->next_pts++;

    // write video frame
    write_frame(dat->oc, vost->enc, vost->st, vost->frame, vost->tmp_pkt);

    if(dat->audio_mod >= 0)
    { // write audio frame
      output_stream_t *ost = &dat->audio_stream;
      AVCodecContext *c;
      AVFrame *frame = ost->tmp_frame;
      int ret;
      int dst_nb_samples;

      c = ost->enc;

      // keep going encoding audio until we're ahead of / equal to video
      while(av_compare_ts(
            dat->video_stream.next_pts, dat->video_stream.enc->time_base,
            dat->audio_stream.next_pts, dat->audio_stream.enc->time_base) > 0) // XXX is this the right time base for next_pts?
      {

        if(!ost->swr_ctx)
          frame = ost->frame;

        int src_nb_samples = frame->nb_samples;
        while(src_nb_samples)
        { // fill exactly the packet size we can get
          uint16_t *samples = 0;
          int sample_cnt = mod->graph->module[dat->audio_mod].so->audio(
              mod->graph->module+dat->audio_mod,
              ost->sample_pos,
              src_nb_samples,
              &samples);
          if(!sample_cnt) goto no_more_audio;
          // TODO: support other stereo/int16 configs!
          memcpy(((int16_t*)frame->data[0]) + 2*(frame->nb_samples - src_nb_samples), samples, 2*sizeof(uint16_t)*sample_cnt);
          ost->sample_pos += sample_cnt;
          src_nb_samples -= sample_cnt;
        }
        frame->pts = ost->next_pts;
        ost->next_pts += frame->nb_samples;

        if(ost->swr_ctx)
        {
          /* convert samples from native format to destination codec format, using the resampler */
          /* compute destination number of samples */
          dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
              c->sample_rate, c->sample_rate, AV_ROUND_UP);
          av_assert0(dst_nb_samples == frame->nb_samples);

          // when we pass a frame to the encoder, it may keep a reference to it
          // internally;
          // make sure we do not overwrite it here
          ret = av_frame_make_writable(ost->frame);
          if (ret < 0) return;

          /* convert to destination format */
          ret = swr_convert(ost->swr_ctx,
              ost->frame->data, dst_nb_samples,
              (const uint8_t **)frame->data, frame->nb_samples);
          if (ret < 0)
          {
            fprintf(stderr, "[o-vid] error while resampling sound\n");
            return;
          }
          frame = ost->frame;

          frame->pts = av_rescale_q(ost->sample_cnt, (AVRational){1, c->sample_rate}, c->time_base);
          ost->sample_cnt += dst_nb_samples;
        }
        else
        {
          dst_nb_samples = frame->nb_samples;
          ost->sample_cnt += dst_nb_samples;
        }

        write_frame(dat->oc, c, ost->st, frame, ost->tmp_pkt);
      } // loop audio packets
no_more_audio:;
    } // end audio frame

    // prepare for next frame
    dat->have_buf[0] = dat->have_buf[1] = dat->have_buf[2] = 0;

    if(mod->graph->frame == mod->graph->frame_cnt-1)
    { // drain packet queues and write trailer etc
      output_stream_t *vs = &dat->video_stream, *as = &dat->audio_stream;
      while(!write_frame(dat->oc, vs->enc, vs->st, 0, vs->tmp_pkt));
      while(!write_frame(dat->oc, as->enc, as->st, 0, as->tmp_pkt));
      close_file(dat);
    }
  }
}
