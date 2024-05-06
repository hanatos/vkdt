#include "modules/api.h"
#include "core/core.h"
#include "core/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// a wrapper around a single output AVStream
typedef struct OutputStream
{
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    AVPacket *tmp_pkt;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
}
OutputStream;

typedef struct buf_t
{
  // TODO
  // avcodec / avformat stuff
  int audio_mod;   // the module on our graph that has the audio
  int have_buf[3]; // flag that we have read the buffers for Y Cb Cr for the given frame
}
buf_t;

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
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t roi_Y  = module->connector[0].roi;
  dt_roi_t roi_CbCr = roi_Y;
  // TODO: if 422 or 420 roi_CbCr.wd /= 2
  // TODO: if 420 roi_CbCr.ht /= 2
  // TODO: pass push constants identifying the buffer input and output characteristics (bit depth, colour space, trc, subsampling)
  const int id_enc = dt_node_add(graph, module, "o-vid", "enc", wd, ht, 1, 0, 0, 4,
      "input", "read", "rgba", "f16", dt_no_roi,
      "Y",     "write", "r",   "ui8", &roi_Y, // TODO: if more than 8 bits, use ui16!
      "Cb",    "write", "r",   "ui8", &roi_CbCr, // TODO: if more than 8 bits, use ui16!
      "Cr",    "write", "r",   "ui8", &roi_CbCr);// TODO: if more than 8 bits, use ui16!
  const int id_Y = dt_node_add(graph, module, "o-vid", "Y", 1, 1, 1, 0, 0, 1,
      "Y", "sink", "r", "ui8", dt_no_roi);
  const int id_Cb = dt_node_add(graph, module, "o-vid", "Cb", 1, 1, 1, 0, 0, 1,
      "Cb", "sink", "r", "ui8", dt_no_roi);
  const int id_Cr = dt_node_add(graph, module, "o-vid", "Cr", 1, 1, 1, 0, 0, 1,
      "Cr", "sink", "r", "ui8", dt_no_roi);
  CONN(dt_node_connect_named(graph, id_enc, "Y",  id_Y,  "Y"));
  CONN(dt_node_connect_named(graph, id_enc, "Cb", id_Cb, "Cb"));
  CONN(dt_node_connect_named(graph, id_enc, "Cr", id_Cr, "Cr"));
  dt_connector_copy(graph, module, 0, id_enc, 0);
}

static inline void
close_stream(AVFormatContext *oc, OutputStream *ost)
{
  avcodec_free_context(&ost->enc);
  av_frame_free(&ost->frame);
  av_frame_free(&ost->tmp_frame);
  av_packet_free(&ost->tmp_pkt);
  // sws_freeContext(ost->sws_ctx);
  swr_free(&ost->swr_ctx);
}

static inline
close_file(buf_t *dat)
{
  // make sure this works even if the file is already closed
  av_write_trailer(oc);

  /* Close each codec. */
  if (have_video) close_stream(oc, &video_st);
  if (have_audio) close_stream(oc, &audio_st);

  if (!(fmt->flags & AVFMT_NOFILE))
    /* Close the output file. */
    avio_closep(&oc->pb);

  /* free the stream */
  avformat_free_context(oc);
}

static void
add_stream(
    OutputStream *ost, AVFormatContext *oc,
    const AVCodec **codec,
    enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

    ost->tmp_pkt = av_packet_alloc();
    if (!ost->tmp_pkt) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        exit(1);
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;
        c->sample_rate = 44100;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        av_channel_layout_copy(&c->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 352;
        c->height   = 288;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        c->time_base       = ost->st->time_base;

        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B-frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
        break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

static inline AVFrame*
alloc_frame(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *frame;
    int ret;

    frame = av_frame_alloc();
    if (!frame)
        return NULL;

    frame->format = pix_fmt;
    frame->width  = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return frame;
}

static inline void
open_video(
    AVFormatContext *oc, const AVCodec *codec,
    OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_frame(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_frame(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary video frame\n");
            exit(1);
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

static inline AVFrame*
alloc_audio_frame(
    enum AVSampleFormat sample_fmt,
    const AVChannelLayout *channel_layout,
    int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }

    frame->format = sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, channel_layout);
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        if (av_frame_get_buffer(frame, 0) < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }

    return frame;
}

static inline void
open_audio(
    AVFormatContext *oc, const AVCodec *codec,
    OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* init signal generator */
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    ost->frame     = alloc_audio_frame(c->sample_fmt, &c->ch_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, &c->ch_layout,
                                       c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        exit(1);
    }

    /* set options */
    av_opt_set_chlayout  (ost->swr_ctx, "in_chlayout",       &c->ch_layout,      0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
    av_opt_set_chlayout  (ost->swr_ctx, "out_chlayout",      &c->ch_layout,      0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        exit(1);
    }
}

static inline int
init_file(buf_t *dat)
{
    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc) return 1;

    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE)
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
    if (fmt->audio_codec != AV_CODEC_ID_NONE)
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);

    open_video(oc, video_codec, &video_st, opt);
    open_audio(oc, audio_codec, &audio_st, opt);

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return 1;
    }
}

// TODO: which data do we need?
// yuv422p10le : y full res, cb and cr half res in x, full res in y
// yuva444p10le : y, cb, cr, alpha all fullres 10 bits padded to 16 bits
// yuv420p for h264, which is the same as in this example, 8bps for all channels
// there is also
// yuv444p16le which we could use as input, but it only scales the data (it is padded to 16 in the case of 10bits)
// TODO: encoding kernel that inputs anything and will output y + chroma texture
// TODO: fill alpha with constant 1023
void write_sink(
    dt_module_t            *mod,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  buf_t *dat = mod->data;
  AVCodecContext *c = ost->enc;

    // XXX TODO: compare to our total length (last timestamp? assume fixed frame rate?)
    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base, STREAM_DURATION, (AVRational){ 1, 1 }) > 0)
        return;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0) return;

    // is what we do below with the memcpy
    // fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);
  if(p->node->kernel == dt_token("Y"))
  {
    // memcpy
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
    dat->have_buf[0] = 1;
  }
  else if(p->node->kernel == dt_token("Cb"))
  { // ffmpeg expects the colour planes separate, not in a 2-channel texture
    // something memcpy with subsampled size. make sure linesize and gpu width match!
            pict->data[1][y * pict->linesize[1] + x] = x + y + i * 3;
    dat->have_buf[1] = 1;
  }
  else if(p->node->kernel == dt_token("Cr"))
  {
    dat->have_buf[2] = 1;
  }
  // alpha has no connector and copies no data

  if(dat->have_buf[0] && dat->have_buf[1] && dat->have_buf[2])
  {
    ost->frame->pts = ost->next_pts++;

    int ret = write_frame(oc, ost->enc, ost->st, ost->frame, ost->tmp_pkt);
    // now it says its finished maybe

    // TODO: we will *not* get an audio callback, we need to grab the samples from the audio module

#if 0
/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, ost->enc->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) > 0)
        return NULL;

    for (j = 0; j <frame->nb_samples; j++) {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->ch_layout.nb_channels; i++)
            *q++ = v;
        ost->t     += ost->tincr;
        ost->tincr += ost->tincr2;
    }

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static inline int
write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    AVCodecContext *c;
    AVFrame *frame;
    int ret;
    int dst_nb_samples;

    c = ost->enc;

    frame = get_audio_frame(ost);

    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                        c->sample_rate, c->sample_rate, AV_ROUND_UP);
        av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
            exit(1);

        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
                          ost->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            exit(1);
        }
        frame = ost->frame;

        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }

    return write_frame(oc, c, ost->st, frame, ost->tmp_pkt);
}
#endif

    // if we have all three channels for a certain frame:
    uint16_t *samples = 0;
    int sample_cnt = mod->graph->module[audio_mod].so->audio(mod->graph->module+audio_mod, mod->graph->frame, &samples);
    // also query img_params for sound properties

    write_audio_frame(oc, &audio_st);
    dat->have_buf[0] = dat->have_buf[1] = dat->have_buf[2] = 0;


    // TODO: init stuff and cleanup stuff?
    // finalise the file:
    if(mod->graph->frame == mod->graph->frame_cnt-1)
      close_file(dat);
  }
}
