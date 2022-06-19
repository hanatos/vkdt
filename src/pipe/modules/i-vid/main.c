// this file is straight in ff hell. let's hope it doesn't get outdated too soon.
// use ffmpeg to read and parse a bit stream
// hand it down to vk video for decoding

#include "modules/api.h"

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <limits.h>
#include <stdint.h>

typedef struct vid_data_t
{
  char                  filename[PATH_MAX];
  AVFormatContext      *fmtc;
  AVPacket             *pkt0, *pkt1, *pktf;
  AVFrame              *aframe, *vframe;
  AVBSFContext         *vbsfc, *absfc;
  int                   audio_idx, video_idx;
  int                   mp4;
  int                   wd, ht;
  AVCodecContext       *actx, *vctx;
  uint32_t              dim[6];
  int64_t               frame;
  float                *sndbuf;
  size_t                sndbuf_size;
  int64_t               snd_lag;
}
vid_data_t;

static inline void
dump_parameters(
    vid_data_t *d)
{
  // enum AVCodecID vcodec = d->fmtc->streams[d->video_idx]->codecpar->codec_id;
  // enum AVPixelFormat format = (enum AVPixelFormat)d->fmtc->streams[d->video_idx]->codecpar->format;
  int bit_depth = 8;
  if(d->fmtc->streams[d->video_idx]->codecpar->format == AV_PIX_FMT_YUV420P10LE)
    bit_depth = 10;
  if(d->fmtc->streams[d->video_idx]->codecpar->format == AV_PIX_FMT_YUV420P12LE)
    bit_depth = 12;
  int profile = d->fmtc->streams[d->video_idx]->codecpar->profile;
  int level = d->fmtc->streams[d->video_idx]->codecpar->level;

  enum AVColorRange color_range = d->fmtc->streams[d->video_idx]->codecpar->color_range;
  AVRational sample_aspect_ratio = d->fmtc->streams[d->video_idx]->codecpar->sample_aspect_ratio;
  enum AVFieldOrder field_order = d->fmtc->streams[d->video_idx]->codecpar->field_order;
  enum AVColorPrimaries color_primaries = d->fmtc->streams[d->video_idx]->codecpar->color_primaries;
  enum AVColorTransferCharacteristic color_trc = d->fmtc->streams[d->video_idx]->codecpar->color_trc;
  enum AVColorSpace color_space = d->fmtc->streams[d->video_idx]->codecpar->color_space;
  enum AVChromaLocation chroma_location = d->fmtc->streams[d->video_idx]->codecpar->chroma_location;
  fprintf(stderr, "[i-vid] %d x %d @ %d profile %d lvl %d aspect %g\n",
      d->wd, d->ht, bit_depth, profile, level, 
      (float)sample_aspect_ratio.num / sample_aspect_ratio.den);

  static const char* fo[] = {
    "UNKNOWN",
    "PROGRESSIVE",
    "TT: Top coded_first, top displayed first",
    "BB: Bottom coded first, bottom displayed first",
    "TB: Top coded first, bottom displayed first",
    "BT: Bottom coded first, top displayed first",
  };
  fprintf(stderr, "[i-vid] field order %s\n", fo[field_order]);

  static const char* cr[] = {
    "UNSPECIFIED",
    "MPEG: the normal 219*2^(n-8) MPEG YUV ranges",
    "JPEG: the normal     2^n-1   JPEG YUV ranges",
    "NB: Not part of ABI",
  };
  fprintf(stderr, "[i-vid] color range %s\n", cr[color_range]);

  static const char* cp[] = {
    "RESERVED0",
    "BT709: also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B",
    "UNSPECIFIED",
    "RESERVED",
    "BT470M: also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)",
    "BT470BG: also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM",
    "SMPTE170M: also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
    "SMPTE240M: also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
    "FILM: colour filters using Illuminant C",
    "BT2020: ITU-R BT2020",
    "SMPTE428: SMPTE ST 428-1 (CIE 1931 XYZ)",
    "SMPTE431: SMPTE ST 431-2 (2011) / DCI P3",
    "SMPTE432: SMPTE ST 432-1 (2010) / P3 D65 / Display P3",
    "JEDEC_P22: JEDEC P22 phosphors",
    "NB: Not part of ABI",
  };
  fprintf(stderr, "[i-vid] colour primaries %s\n", cp[color_primaries]);

  static const char* ctrc[] = {
    "RESERVED0",
    "BT709: also ITU-R BT1361",
    "UNSPECIFIED",
    "RESERVED",
    "GAMMA22:  also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM",
    "GAMMA28:  also ITU-R BT470BG",
    "SMPTE170M:  also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC",
    "SMPTE240M",
    "LINEAR:  Linear transfer characteristics",
    "LOG: Logarithmic transfer characteristic (100:1 range)",
    "LOG_SQRT: Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)",
    "IEC61966_2_4: IEC 61966-2-4",
    "BT1361_ECG: ITU-R BT1361 Extended Colour Gamut",
    "IEC61966_2_1: IEC 61966-2-1 (sRGB or sYCC)",
    "BT2020_10: ITU-R BT2020 for 10-bit system",
    "BT2020_12: ITU-R BT2020 for 12-bit system",
    "SMPTE2084: SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems",
    "SMPTE428:  SMPTE ST 428-1",
    "ARIB_STD_B67:  ARIB STD-B67, known as Hybrid log-gamma",
    "NB: Not part of ABI",
  };
  fprintf(stderr, "[i-vid] trc %s\n", ctrc[color_trc]);

  static const char* cs[] = {
    "RGB:   order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)",
    "BT709:   also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B",
    "UNSPECIFIED",
    "RESERVED",
    "FCC:  FCC Title 47 Code of Federal Regulations 73.682 (a)(20)",
    "BT470BG:  also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601",
    "SMPTE170M:  also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
    "SMPTE240M:  functionally identical to above",
    "YCGCO:  Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16",
    "BT2020_NCL:  ITU-R BT2020 non-constant luminance system",
    "BT2020_CL:  ITU-R BT2020 constant luminance system",
    "SMPTE2085:  SMPTE 2085, Y'D'zD'x",
    "CHROMA_DERIVED_NCL:  Chromaticity-derived non-constant luminance system",
    "CHROMA_DERIVED_CL:  Chromaticity-derived constant luminance system",
    "ICTCP:  ITU-R BT.2100-0, ICtCp",
    "NB:  Not part of ABI",
  };
  fprintf(stderr, "[i-vid] colour space %s\n", cs[color_space]);

  static const char* cl[] = {
    "UNSPECIFIED",
    "LEFT: MPEG-2/4 4:2:0, H.264 default for 4:2:0",
    "CENTER: MPEG-1 4:2:0, JPEG 4:2:0, H.263 4:2:0",
    "TOPLEFT: ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2",
    "TOP",
    "BOTTOMLEFT",
    "BOTTOM",
    "NB:Not part of ABI",
  };
  fprintf(stderr, "[i-vid] chroma location %s\n", cl[chroma_location]);
}

static inline int
open_stream(vid_data_t *d, const char *filename)
{
  if(!strcmp(d->filename, filename)) return 0; // already opened this stream
  memset(d, 0, sizeof(*d));

  int ret = 0;
  fprintf(stderr, "[i-vid] trying to open %s\n", filename);
  if((ret = avformat_open_input(&d->fmtc, filename, 0, 0)) < 0) goto error;
  if(!d->fmtc) goto error;
  fprintf(stderr, "[i-vid] loaded %s format %s (%s)\n", filename, d->fmtc->iformat->long_name, d->fmtc->iformat->name);

  if((ret = avformat_find_stream_info(d->fmtc, 0)) < 0) goto error;
  d->audio_idx = av_find_best_stream(d->fmtc, AVMEDIA_TYPE_AUDIO, -1, -1, 0, 0);
  d->video_idx = av_find_best_stream(d->fmtc, AVMEDIA_TYPE_VIDEO, -1, -1, 0, 0);
  if(d->video_idx < 0) goto error;

  d->mp4 = (// !strcmp(d->fmtc->iformat->long_name, "QuickTime / MOV") ||
            !strcmp(d->fmtc->iformat->long_name, "FLV (Flash Video)") ||
            !strcmp(d->fmtc->iformat->long_name, "Matroska / WebM"));

  d->wd = d->fmtc->streams[d->video_idx]->codecpar->width;
  d->ht = d->fmtc->streams[d->video_idx]->codecpar->height;
  d->dim[0] = d->wd;
  d->dim[1] = d->ht;
  d->dim[2] = d->wd/2;
  d->dim[3] = d->ht/2;
  d->dim[4] = d->wd/2;
  d->dim[5] = d->ht/2;

  d->pkt0 = av_packet_alloc();
  d->pkt1 = av_packet_alloc();
  d->pktf = av_packet_alloc();
  enum AVCodecID acodec = d->audio_idx >= 0 ? d->fmtc->streams[d->audio_idx]->codecpar->codec_id : 0;
  enum AVCodecID vcodec = d->fmtc->streams[d->video_idx]->codecpar->codec_id;

  const AVBitStreamFilter *bsf = 0;
  if(d->mp4)
  {
    if (vcodec == AV_CODEC_ID_H264)
      bsf = av_bsf_get_by_name("h264_mp4toannexb");
    else if(vcodec == AV_CODEC_ID_HEVC)
      bsf = av_bsf_get_by_name("hevc_mp4toannexb");
    else if(vcodec == AV_CODEC_ID_VP9)
      d->mp4 = 0;
  }

  if(d->mp4)
  {
    if(!bsf)
    {
      fprintf(stderr, "[i-vid] av_bsf_get_by_name failed!\n");
      goto error;
    }
    if((ret = av_bsf_alloc(bsf, &d->vbsfc)) < 0) goto error;
    avcodec_parameters_copy(d->vbsfc->par_in, d->fmtc->streams[d->video_idx]->codecpar);
    if((ret = av_bsf_init(d->vbsfc)) < 0) goto error;

#if 1
    if(d->audio_idx >= 0)
    { // now the audio
      if (acodec == AV_CODEC_ID_AAC)
        bsf = av_bsf_get_by_name("aac_adtstoasc");
      if(!bsf)
      {
        fprintf(stderr, "[i-vid] av_bsf_get_by_name failed for audio!\n");
        goto error;
      }
      if((ret = av_bsf_alloc(bsf, &d->absfc)) < 0) goto error;
      avcodec_parameters_copy(d->absfc->par_in, d->fmtc->streams[d->audio_idx]->codecpar);
      if((ret = av_bsf_init(d->absfc)) < 0) goto error;
    }
#endif
  }
  AVDictionary *opts = 0;
  // av_dict_set(&opts, "b", "2.5M", 0); // ??
  const AVCodec *v_codec = avcodec_find_decoder(vcodec);
  d->vctx = avcodec_alloc_context3(v_codec);
  if((ret = avcodec_parameters_to_context(d->vctx, d->fmtc->streams[d->video_idx]->codecpar)) < 0) goto error;
  if((ret = avcodec_open2(d->vctx, v_codec, &opts)) < 0) goto error;
  if(d->audio_idx >= 0)
  {
    const AVCodec *a_codec = avcodec_find_decoder(acodec);
    if(!a_codec) fprintf(stderr, "[i-vid] could not find audio codec!\n");
    d->actx = avcodec_alloc_context3(a_codec);
    if((ret = avcodec_parameters_to_context(d->actx, d->fmtc->streams[d->audio_idx]->codecpar)) < 0) goto error;
    if((ret = avcodec_open2(d->actx, a_codec, &opts)) < 0) goto error;
  }

  d->vframe = av_frame_alloc();
  d->aframe = av_frame_alloc();

  fprintf(stderr, "[i-vid] successfully opened %s %dx%d\n", filename, d->wd, d->ht);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation" // yes i know it may truncate stfu
  strncpy(d->filename, filename, sizeof(d->filename));
#pragma GCC diagnostic pop
  dump_parameters(d);
  return 0;
error:
  fprintf(stderr, "[i-vid] error opening %s (%s)\n", filename, av_err2str(ret));
  memset(d, 0, sizeof(*d));
  return 1;
}

int init(dt_module_t *mod)
{
  vid_data_t *d = calloc(sizeof(*d), 1);
  mod->data = d;
  mod->flags = s_module_request_read_source;
  return 0;
}

static inline void
close_stream(vid_data_t *d)
{
  if(!d) return;
  if(!d->filename[0]) return;

  av_frame_free(&d->aframe);
  av_frame_free(&d->vframe);
  if(d->pkt0->data) av_packet_unref(d->pkt0);
  if(d->pkt1->data) av_packet_unref(d->pkt1); 
  if(d->pktf->data) av_packet_unref(d->pktf);
  av_packet_free(&d->pkt0);
  av_packet_free(&d->pkt1);
  av_packet_free(&d->pktf);

  avformat_close_input(&d->fmtc);
  if(d->mp4) av_bsf_free(&d->vbsfc);
  if(d->mp4) av_bsf_free(&d->absfc);
  avcodec_close(d->vctx); // will clean up codec, not context
  avcodec_free_context(&d->vctx);
  avcodec_close(d->actx);
  avcodec_free_context(&d->actx);
  memset(d, 0, sizeof(*d));
}

void cleanup(dt_module_t *mod)
{
  vid_data_t *d = mod->data;
  free(d->sndbuf);
  close_stream(d);
  free(d);
  mod->data = 0;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  const char *fname = dt_module_param_string(mod, 0);
  const char *filename = fname;
  char tmpfn[2*PATH_MAX+10];
  if(filename[0] != '/') // relative paths
  {
    snprintf(tmpfn, sizeof(tmpfn), "%s/%s", mod->graph->searchpath, fname);
    filename = tmpfn;
    FILE *f = fopen(filename, "rb");
    if(!f)
    {
      snprintf(tmpfn, sizeof(tmpfn), "%s/%s", mod->graph->basedir, fname);
      filename = tmpfn;
      f = fopen(filename, "rb");
      if(!f) return; // damn that.
    }
    fclose(f);
  }

  vid_data_t *d = mod->data;
  if(open_stream(d, filename)) return;
  mod->connector[0].roi.full_wd = d->wd;
  mod->connector[0].roi.full_ht = d->ht;
  // TODO: also init these correctly (as good as we can):
  float b = 0.0, w = 1.0f;
  mod->img_param = (dt_image_params_t) {
    .black          = {b, b, b, b},
    .white          = {w, w, w, w},
    .whitebalance   = {1.0, 1.0, 1.0, 1.0},
    .filters        = 0, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, d->wd, d->ht},

    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},
    .orientation    = 0,
    // .exposure       = dat->video.EXPO.shutterValue * 1e-6f,
    // .aperture       = dat->video.LENS.aperture * 0.01f,
    // .iso            = dat->video.EXPO.isoValue,
    // .focal_length   = dat->video.LENS.focalLength,

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  // snprintf(mod->img_param.datetime, sizeof(mod->img_param.datetime), "%4d%2d%2d %2d:%2d%2d",
  //     dat->video.RTCI.tm_year, dat->video.RTCI.tm_mon, dat->video.RTCI.tm_mday,
  //     dat->video.RTCI.tm_hour, dat->video.RTCI.tm_min, dat->video.RTCI.tm_sec);
  // snprintf(mod->img_param.model, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  // snprintf(mod->img_param.maker, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  // for(int i=0;i<sizeof(mod->img_param.maker);i++) if(mod->img_param.maker[i] == ' ') mod->img_param.maker[i] = 0;
  double frame_rate = av_q2d(d->fmtc->streams[d->video_idx]->avg_frame_rate);
  double duration = d->fmtc->duration / (double)AV_TIME_BASE; // in seconds
  mod->graph->frame_cnt = duration * frame_rate;
  // XXX FIXME: the number is correct but needs more testing because
  // we can't deliver 60fps on slower computers, killing audio etc
  // this first needs a robust way of doing frame drops.
  // if(mod->graph->frame_rate == 0) // don't overwrite cfg
    // mod->graph->frame_rate = frame_rate;
}

#if 0 // TODO
int read_vid(
    dt_module_t          *mod,
    void                 *mapped,
    dt_read_vid_params_t *p)
#endif
int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  vid_data_t *d = mod->data;
  if(!d->filename[0]) return 1; // not open

  int ret = 0;

  if(p->a == 0)
  { // first channel, new frame. parse + decode + handle audio:
    const double tbn = 1.0/av_q2d(d->fmtc->streams[d->video_idx]->time_base);
    int rate = tbn/mod->graph->frame_rate; // some obscure sampling rate vs frames per second number
    // fprintf(stderr, "frames %d %ld %d %d\n", mod->graph->frame, d->frame, d->vctx->frame_number, vfidx);//d->vframe->pts);
    if(mod->graph->frame != d->frame)
    // if(mod->graph->frame + 1 != d->vctx->frame_number) // zero vs 1 based
    { // seek
      // old api. passing -1 converts the timestamp to something even more obscure.
      // passing video idx seeks to somewhere about the right place (+10 frames or so)
      int64_t dts = mod->graph->frame * rate;
      if((ret = av_seek_frame(d->fmtc, d->video_idx, dts, AVSEEK_FLAG_ANY)) < 0) goto error;
#if 0 // new api does not seem to work yet:
      if((ret = avformat_seek_file(d->fmtc, -1,
              // mod->graph->frame - 2, mod->graph->frame, mod->graph->frame + 2,
              0, mod->graph->frame, mod->graph->frame + 2,
              AVSEEK_FLAG_ANY|AVSEEK_FLAG_FRAME)) < 0) goto error;
#endif
      // fprintf(stderr, "SEEEEEEEKX\n");
      avcodec_flush_buffers(d->vctx);
      avcodec_flush_buffers(d->actx);
      d->snd_lag = 0;
    }
    d->frame = mod->graph->frame+1; // this would be the next one we read

    AVPacket *curr = d->pkt0;
    do {
      if(d->pkt0->data) av_packet_unref(d->pkt0);
      if(d->pkt1->data) av_packet_unref(d->pkt1);
      if((ret = avcodec_receive_frame(d->vctx, d->vframe)) < 0)
      {
        if(ret == AVERROR(EAGAIN))
        { // receive frame needs moar packets!
          if((ret = av_read_frame(d->fmtc, curr)) < 0)
          { // this would have to be EOF hopefully
            if(ret == AVERROR_EOF) break;
            goto error;
          }
          if(curr->stream_index == d->video_idx)
          {
            AVPacket *pk = curr;
            if(d->mp4)
            {
              if(d->pktf->data) av_packet_unref(d->pktf);
              if((ret = av_bsf_send_packet   (d->vbsfc, curr))    < 0) goto error;
              if((ret = av_bsf_receive_packet(d->vbsfc, d->pktf)) < 0) goto error;
              pk = d->pktf;
            }
            if((ret = avcodec_send_packet(d->vctx, pk)) < 0)
            {
              if(ret == AVERROR(EAGAIN)) {} // internal buffer is full, stop sending! first pick it up.
              goto error;
            }
          }
          if(curr->stream_index == d->audio_idx)
          {
#if 0
            AVPacket *pk = curr;
            if(d->mp4)
            {
              if(d->pktf->data) av_packet_unref(d->pktf);
              if((ret = av_bsf_send_packet   (d->absfc, curr))    < 0) goto error;
              if((ret = av_bsf_receive_packet(d->absfc, d->pktf)) < 0) goto error;
              pk = d->pktf;
            }
#endif
            if((ret = avcodec_send_packet(d->actx, curr)) < 0)
            {
              if(ret == AVERROR(EAGAIN)) {} // all good, buffer full already
              else goto error;
            }
          }
          continue; // go on receive a frame now
        }
        else if(ret == AVERROR_EOF)
        { // receive frame says EOF, flush empty packet
          if((ret = avcodec_send_packet(d->vctx, 0)) < 0) goto error;
          continue;
        }
        goto error; // seems to be broken indeed.
      }
      break; // got frame, exit loop
    } while(1);
    // int64_t vfidx = d->vframe->pts;//  / rate;
    // fprintf(stderr, "frames %d %ld %d %ld\n", mod->graph->frame, d->frame, d->vctx->frame_number, vfidx);//d->vframe->pts);
  }

  // write the frame data to output file
  uint32_t wd = p->a ? d->wd/2 : d->wd;
  uint32_t ht = p->a ? d->ht/2 : d->ht;
  if(d->vframe->linesize[p->a])
    for(int j=0;j<ht;j++)
      memcpy(mapped + sizeof(uint8_t) * wd * j, d->vframe->data[p->a] +sizeof(uint8_t)*d->vframe->linesize[p->a]*j, wd);

  if(p->a == 2) av_frame_unref(d->vframe);

  return 0;
error:
  fprintf(stderr, "[i-vid] error during decoding (%s)\n", av_err2str(ret));
  return 1;
}

int audio(
    dt_module_t  *mod,
    const int     frame,
    uint8_t     **samples)
{
  vid_data_t *d = mod->data;
  if(!d || !d->filename[0])
    return 0;
  int ret = 0;
  int written = 0;
  int num_samples = -1;
  int need_samples = -1;
  *samples = (uint8_t *)d->sndbuf;
  do {
    if((ret = avcodec_receive_frame(d->actx, d->aframe)) < 0)
    {
      if(ret == AVERROR(EAGAIN)) { } // receive frame needs moar packets! but sorry the main loop is in read_source()
      return written; // got zero samples in the last round
    }
#if 0
      fprintf(stderr, "frame %d, lag %ld got audio with %d channels, lay %lu, %d samples, %s format, rate %d\n",
          // d->aframe->pts,
          mod->graph->frame,
          d->snd_lag,
          d->aframe->channels, d->aframe->channel_layout, // layout 3 means front left | front right (= 1 | 2)
          d->aframe->nb_samples,
          av_get_sample_fmt_name(d->aframe->format), // fltp means floating point -1..1 in planes.
          d->aframe->sample_rate);
#endif
    const float *input_l = (const float *)d->aframe->extended_data[0];
    const float *input_r = (const float *)d->aframe->extended_data[1];
    if(!input_r) input_r = input_l;

    if(num_samples == -1)
    {
      float frame_rate = mod->graph->frame_rate;
      if(frame_rate < 1) return 0; // no fixed frame rate no sound
      num_samples = d->aframe->sample_rate / mod->graph->frame_rate + 0.5; // how many per one video frame?
      // num_samples = 44100 / mod->graph->frame_rate + 0.5; // how many per one video frame?
      need_samples = num_samples - d->snd_lag; // how many do we need to also compensate the lag?
      size_t sizereq = 3 * MAX(d->aframe->nb_samples, need_samples) * d->aframe->channels;
      if(d->sndbuf_size < sizereq)
      {
        free(d->sndbuf);
        d->sndbuf_size = sizereq;
        d->sndbuf = malloc(d->sndbuf_size * sizeof(float));
        *samples = (uint8_t *)d->sndbuf;
      }
    }
    for(int i=0;i<d->aframe->nb_samples;i++)
    {
      d->sndbuf[2*(i+written)+0] = input_l[i];
      d->sndbuf[2*(i+written)+1] = input_r[i];
    }
    written += d->aframe->nb_samples;
    d->snd_lag = written - need_samples;
  } while(d->snd_lag < 0);
  return written;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  vid_data_t *d = module->data;
  // put compute shader to convert three plane yuv to rgba f16
  assert(graph->num_nodes < graph->max_nodes);
  const int id_in = graph->num_nodes++;
  graph->node[id_in] = (dt_node_t) {
    .name   = dt_token("i-vid"),
    .kernel = dt_token("source"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .flags  = s_module_request_read_source,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("source"),
      .type   = dt_token("source"),
      .chan   = dt_token("y"),
      .format = dt_token("ui8"),
      .roi    = module->connector[0].roi,
      .array_length = 3,
      .array_dim    = d->dim,
    }},
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_conv = graph->num_nodes++;
  graph->node[id_conv] = (dt_node_t) {
    .name   = dt_token("i-vid"),
    .kernel = dt_token("conv"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("y"),
      .format = dt_token("ui8"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
      .array_length = 3,
      .array_dim    = d->dim,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };
  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_conv, 1);
  dt_node_connect  (graph, id_in,  0, id_conv, 0);
}









#if 0
// TODO: this needs to be set on the node video data after opening!
inline VkVideoCodecOperationFlagBitsKHR FFmpeg2NvCodecId(AVCodecID id) {
    switch (id) {
    case AV_CODEC_ID_MPEG1VIDEO : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_MPEG2VIDEO : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_MPEG4      : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_VC1        : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_H264       : return VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT;
    case AV_CODEC_ID_HEVC       : return VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_EXT;
    case AV_CODEC_ID_VP8        : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
#ifdef VK_EXT_video_decode_vp9
    case AV_CODEC_ID_VP9        : return VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
#endif // VK_EXT_video_decode_vp9
    case AV_CODEC_ID_MJPEG      : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
    default                     : assert(false); return VkVideoCodecOperationFlagBitsKHR(0);
    }
}
#endif
