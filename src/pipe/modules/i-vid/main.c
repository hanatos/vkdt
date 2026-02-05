// this file is straight in ff hell. let's hope it doesn't get outdated too soon.
// use ffmpeg to read and parse a bit stream
// hand it down to vk video for decoding

#include "modules/api.h"
#include "core/fs.h"

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

  int p_chroma;
  int p_bits;
}
vid_data_t;

int av_sample_fmt_to_alsa(int sf)
{
  switch(sf)
  {
    case AV_SAMPLE_FMT_NONE: return -1;
    case AV_SAMPLE_FMT_U8:   return  1;
    case AV_SAMPLE_FMT_S16:  return  2;
    case AV_SAMPLE_FMT_S32:  return 10;
    case AV_SAMPLE_FMT_FLT:  return 14;
    case AV_SAMPLE_FMT_DBL:  return 16;
    default: return -1; // uhm
                        // case AV_SAMPLE_FMT_U8P:
                        // case AV_SAMPLE_FMT_S16P:
                        // case AV_SAMPLE_FMT_S32P:
                        // case AV_SAMPLE_FMT_FLTP:
                        // case AV_SAMPLE_FMT_DBLP:
                        // case AV_SAMPLE_FMT_S64:
                        // case AV_SAMPLE_FMT_S64P:
  }
}

static inline void
parse_parameters(
    dt_module_t *mod,
    vid_data_t  *d)
{
  int *p_colour   = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("colour")));
  int *p_trc      = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("trc")));
  int *p_bits     = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("bitdepth")));
  int *p_chroma   = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("chroma")));
  int *p_colrange = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("colrange")));

  // if(p_bits[0] == 4) // we *always* overwrite this because it will crash if a user sets this to a wrong thing!
  switch(d->fmtc->streams[d->video_idx]->codecpar->format)
  {
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV422P:
      p_bits[0] = 0;
      break;
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV422P10LE:
      p_bits[0] = 1;
      break;
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV420P12LE:
      p_bits[0] = 2;
      break;
    case AV_PIX_FMT_YUV420P16LE:
    case AV_PIX_FMT_YUV422P16LE:
      p_bits[0] = 3;
      break;
    default:
      p_bits[0] = 4; // unsupported
  }
  // if(p_chroma[0] == 3) // we *always* overwrite this because it will crash if a user sets this to a wrong thing!
  switch(d->fmtc->streams[d->video_idx]->codecpar->format)
  {
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV420P16LE:
      p_chroma[0] = 0;
      break;
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV422P16LE:
      p_chroma[0] = 1;
      break;
    default:
      p_chroma[0] = 3; // unsupported
  }
  if(p_colrange[0] == 2)
  switch(d->fmtc->streams[d->video_idx]->codecpar->color_range)
  { 
    case AVCOL_RANGE_MPEG:
      p_colrange[0] = 0;
      break;
    case AVCOL_RANGE_UNSPECIFIED:
    case AVCOL_RANGE_JPEG:
      p_colrange[0] = 1;
      break;
    default:
      p_colrange[0] = 1; // default to jpeg/full range
  }

  // enum AVCodecID vcodec = d->fmtc->streams[d->video_idx]->codecpar->codec_id;
  // enum AVPixelFormat format = (enum AVPixelFormat)d->fmtc->streams[d->video_idx]->codecpar->format;
  const int bd[] = {8, 10, 12, 16, -1};
  int bit_depth = bd[p_bits[0]];

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
  if(p_trc[0] == 4)
  {
    p_trc[0] = 1; // default to bt.709
    if(color_trc == 8)  p_trc[0] = 0; // linear
    if(color_trc == 1)  p_trc[0] = 1; // bt.709
    if(color_trc == 16) p_trc[0] = 2; // smpte 2084
    if(color_trc == 18) p_trc[0] = 3; // HLG
  }

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
  if(p_colour[0] == 2)
  {
    p_colour[0] = 0; // default to bt.709
    if(color_space == 1) p_colour[0] = 0; // bt.709
    if(color_space == 9) p_colour[0] = 1; // bt.2020 non constant luminance
  }

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
  d->p_chroma = p_chroma[0];
  d->p_bits   = p_bits[0];
}

static inline int
open_stream(
    vid_data_t  *d,
    dt_module_t *mod,
    const char  *filename)
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

  parse_parameters(mod, d);
  d->wd = d->fmtc->streams[d->video_idx]->codecpar->width;
  d->ht = d->fmtc->streams[d->video_idx]->codecpar->height;
  d->dim[0] = d->wd;
  d->dim[1] = d->ht;
  d->dim[2] = d->p_chroma < 2 ? d->wd/2 : d->wd;
  d->dim[3] = d->p_chroma < 1 ? d->ht/2 : d->ht;
  d->dim[4] = d->p_chroma < 2 ? d->wd/2 : d->wd;
  d->dim[5] = d->p_chroma < 1 ? d->ht/2 : d->ht;

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

  size_t r = snprintf(d->filename, sizeof(d->filename), "%s", filename);
  if(r >= sizeof(d->filename)) d->filename[sizeof(d->filename)-1] = 0;
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
  avcodec_free_context(&d->vctx);
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
  const char *fname = dt_module_param_string(mod, dt_module_get_param(mod->so, dt_token("filename")));
  char filename[PATH_MAX];
  if(dt_graph_get_resource_filename(mod, fname, 0, filename, sizeof(filename)))
    return;

  vid_data_t *d = mod->data;
  if(open_stream(d, mod, filename)) return;
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

    .snd_samplerate = d->actx ? d->actx->sample_rate : 0,
    .snd_format     = av_sample_fmt_to_alsa(d->actx ? d->actx->sample_fmt : -1),
    .snd_channels   = d->actx ? d->actx->ch_layout.nb_channels : 0,

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  fs_createdate(filename, mod->img_param.datetime);
  // snprintf(mod->img_param.model, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  // snprintf(mod->img_param.maker, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  // for(int i=0;i<sizeof(mod->img_param.maker);i++) if(mod->img_param.maker[i] == ' ') mod->img_param.maker[i] = 0;
  double frame_rate = av_q2d(d->fmtc->streams[d->video_idx]->avg_frame_rate);
  double duration = d->fmtc->duration / (double)AV_TIME_BASE; // in seconds
  mod->graph->frame_cnt = duration * frame_rate - 2;
  // XXX FIXME: the number is correct but needs more testing because
  // we can't deliver 60fps on slower computers, killing audio etc
  // this first needs a robust way of doing frame drops.
  if(mod->graph->frame_rate == 0) // don't overwrite cfg
    mod->graph->frame_rate = frame_rate;
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
    if(mod->graph->frame != d->vctx->frame_num) // zero vs 1 based, will increment frame_num when decoding below
    { // seek
      double rate = AV_TIME_BASE / mod->graph->frame_rate;
      int ts = mod->graph->frame * rate;
      if(avformat_seek_file(d->fmtc, -1, ts-2, ts, ts+2, AVSEEK_FLAG_ANY))
        goto error;
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
            if(d->actx && ((ret = avcodec_send_packet(d->actx, curr)) < 0))
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
    // fprintf(stderr, "frames %d %ld %d %ld\n", mod->graph->frame, d->frame, d->vctx->frame_num, vfidx);//d->vframe->pts);
  }

  // write the frame data to output file
  if(d->vframe->linesize[p->a])
  {
    const int p_bits   = d->p_bits;
    const int p_chroma = d->p_chroma;
    int wd = (p->a && (p_chroma < 2)) ? d->wd/2 : d->wd, ht = (p->a && (p_chroma < 1)) ? d->ht/2 : d->ht;
    if(p_bits == 0)
    { // 8-bit
      for(int j=0;j<ht;j++)
        memcpy(mapped + sizeof(uint8_t) * wd * j, d->vframe->data[p->a] + d->vframe->linesize[p->a]*j, wd);
    }
    else if(p_bits == 1 || p_bits == 2 || p_bits == 3)
    { // 16-bit
      for(int j=0;j<ht;j++)
        memcpy(mapped + sizeof(uint16_t) * wd * j, d->vframe->data[p->a] + d->vframe->linesize[p->a]*j, wd * sizeof(uint16_t));
    }
  }

  // fprintf(stderr, "frame %d %ld %ld channel %d linesize %d ret %s \n",
  //     mod->graph->frame, d->frame, d->vctx->frame_num,
  //     p->a, d->vframe->linesize[p->a], av_err2str(ret));

  if(p->a == 2) av_frame_unref(d->vframe);

  return 0;
error:
  fprintf(stderr, "[i-vid] error during decoding (%s)\n", av_err2str(ret));
  return 1;
}

int audio(
    dt_module_t  *mod,
    uint64_t      sample_beg,
    uint32_t      sample_cnt,
    uint8_t     **samples)
{
  vid_data_t *d = mod->data;
  if(!d || !d->filename[0] || !d->actx)
    return 0;
  int ret = 0;
  int written = 0;
  int num_samples = -1;
  int need_samples = -1;
  *samples = (uint8_t *)d->sndbuf;
  do {
    if(d->actx && ((ret = avcodec_receive_frame(d->actx, d->aframe)) < 0))
    {
      if(ret == AVERROR(EAGAIN)) { } // receive frame needs moar packets! but sorry the main loop is in read_source()
      return written; // got zero samples in the last round
    }
    const float *input_l = (const float *)d->aframe->extended_data[0];
    const float *input_r = (const float *)d->aframe->extended_data[1];
    if(!input_r) input_r = input_l;

    int channels = d->actx->ch_layout.nb_channels;
    size_t bps = 1; // u8
    if     (mod->graph->main_img_param.snd_format ==  2) bps = 2; // S16
    else if(mod->graph->main_img_param.snd_format == 10) bps = 4; // S32
    else if(mod->graph->main_img_param.snd_format == 14) bps = 4; // f32
    else if(mod->graph->main_img_param.snd_format == 16) bps = 8; // d32

    if(num_samples == -1)
    {
      float frame_rate = mod->graph->frame_rate;
      if(frame_rate < 1) return 0; // no fixed frame rate no sound
      num_samples = d->aframe->sample_rate / mod->graph->frame_rate + 0.5; // how many per one video frame?
      // num_samples = 44100 / mod->graph->frame_rate + 0.5; // how many per one video frame?
      need_samples = num_samples - d->snd_lag; // how many do we need to also compensate the lag?
      size_t sizereq = 3 * MAX(d->aframe->nb_samples, need_samples) * bps * channels;
      if(d->sndbuf_size < sizereq)
      {
        free(d->sndbuf);
        d->sndbuf_size = sizereq;
        d->sndbuf = malloc(d->sndbuf_size * sizeof(float));
        *samples = (uint8_t *)d->sndbuf;
      }
    }
    memcpy(((uint8_t*)d->sndbuf) + written*bps*channels, input_l, d->aframe->nb_samples * bps * channels);
    written += d->aframe->nb_samples;
    d->snd_lag = written - need_samples; // negative snd_lag means we're behind video, assuming one audio() call per frame (i.e. sample_cnt == 0)
  } while((sample_cnt == 0 && d->snd_lag < 0) || (sample_cnt > 0 && written < sample_cnt));
  return written;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  vid_data_t *d = module->data;
  // put compute shader to convert three plane yuv to rgba f16
  const int id_in = dt_node_add(graph, module, "i-vid", "source",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 1,
      "source", "source", "y", d->p_bits ? "ui16" : "ui8", &module->connector[0].roi);
  graph->node[id_in].flags = s_module_request_read_source;
  graph->node[id_in].connector[0].array_length = 3;
  graph->node[id_in].connector[0].array_dim = d->dim;

  const int id_conv = dt_node_add(graph, module, "i-vid", "conv",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "y", d->p_bits ? "ui16" : "ui8", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
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
