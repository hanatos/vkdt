#include "modules/api.h"
#include "core/fs.h"
#include "vid.h"
#include "vid-init.c"
#include "vid-run.c"

#include <stdint.h>

typedef struct vid_data_t
{
  char           filename[PATH_MAX];
  int            nid;
  int            frame;
  decode_video_t v;
}
vid_data_t;

#if 1 // XXX TODO some of that makes sense (detect colour/trc)
// XXX some of it is automatic in the ycbcr conversion (chroma/bitdepth)
// XXX some is automatic and might still be exposed (colrange)
static inline void
parse_parameters(
    dt_module_t *mod,
    vid_data_t  *d)
{
  int *p_colour   = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("colour")));
  int *p_trc      = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("trc")));
  // these are for information/read only:
  int *p_bits     = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("bitdepth")));
  int *p_chroma   = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("chroma")));
  int *p_colrange = (int *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("colrange")));

  switch(d->v.video.av_stream->codecpar->format)
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
  switch(d->v.video.av_stream->codecpar->format)
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
  switch(d->v.video.av_stream->codecpar->color_range)
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

#if 0
  // enum AVCodecID vcodec = d->fmtc->streams[d->video_idx]->codecpar->codec_id;
  // enum AVPixelFormat format = (enum AVPixelFormat)d->fmtc->streams[d->video_idx]->codecpar->format;
  const int bd[] = {8, 10, 12, 16, -1};
  int bit_depth = bd[p_bits[0]];

  int profile = d->fmtc->streams[d->video_idx]->codecpar->profile;
  int level = d->fmtc->streams[d->video_idx]->codecpar->level;

  enum AVColorRange color_range = d->fmtc->streams[d->video_idx]->codecpar->color_range;
  AVRational sample_aspect_ratio = d->fmtc->streams[d->video_idx]->codecpar->sample_aspect_ratio;
  enum AVFieldOrder field_order = d->fmtc->streams[d->video_idx]->codecpar->field_order;
  enum AVChromaLocation chroma_location = d->fmtc->streams[d->video_idx]->codecpar->chroma_location;
#endif
  enum AVColorPrimaries color_primaries = d->v.video.av_stream->codecpar->color_primaries;
  enum AVColorTransferCharacteristic color_trc = d->v.video.av_stream->codecpar->color_trc;
#if 0
  enum AVColorSpace color_space = d->fmtc->streams[d->video_idx]->codecpar->color_space;
  fprintf(stderr, "[i-vid] %d x %d @ %d profile %d lvl %d aspect %g\n",
      d->wd, d->ht, bit_depth, profile, level, 
      (float)sample_aspect_ratio.num / sample_aspect_ratio.den);
#endif

#if 0
  static const char* fo[] = {
    "UNKNOWN",
    "PROGRESSIVE",
    "TT: Top coded_first, top displayed first",
    "BB: Bottom coded first, bottom displayed first",
    "TB: Top coded first, bottom displayed first",
    "BT: Bottom coded first, top displayed first",
  };
  fprintf(stderr, "[i-vid] field order %s\n", fo[field_order]);
#endif

#if 0
  static const char* cr[] = {
    "UNSPECIFIED",
    "MPEG: the normal 219*2^(n-8) MPEG YUV ranges",
    "JPEG: the normal     2^n-1   JPEG YUV ranges",
    "NB: Not part of ABI",
  };
  fprintf(stderr, "[i-vid] color range %s\n", cr[color_range]);
#endif

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
  if(p_colour[0] == -1)
  {
    p_colour[0] = s_colour_primaries_srgb; // default to bt.709
    if(color_primaries ==  1) p_colour[0] = s_colour_primaries_srgb; // bt.709
    if(color_primaries ==  9) p_colour[0] = s_colour_primaries_2020; // bt.2020 non constant luminance
    if(color_primaries == 10) p_colour[0] = s_colour_primaries_XYZ;  // cie xyz
    if(color_primaries == 12) p_colour[0] = s_colour_primaries_P3;   // display P3
  }

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
  if(p_trc[0] == -1)
  {
    p_trc[0] = 1; // default to bt.709
    if(color_trc ==  8) p_trc[0] = s_colour_trc_linear; // linear
    if(color_trc ==  4) p_trc[0] = s_colour_trc_gamma;  // gamma22
    if(color_trc ==  1) p_trc[0] = s_colour_trc_709;    // bt.709
    if(color_trc == 13) p_trc[0] = s_colour_trc_srgb;   // sRGB
    if(color_trc == 16) p_trc[0] = s_colour_trc_PQ;     // smpte 2084
    if(color_trc == 18) p_trc[0] = s_colour_trc_HLG;    // HLG
  }

#if 0
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
  if(p_colour[0] == -1)
  {
    p_colour[0] = s_colour_primaries_srgb; // default to bt.709
    if(color_space == 1) p_colour[0] = s_colour_primaries_srgb; // bt.709
    if(color_space == 9) p_colour[0] = s_colour_primaries_2020; // bt.2020 non constant luminance
  }
#endif

#if 0
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
#endif
}
#endif

int init(dt_module_t *mod)
{
  vid_data_t *d = calloc(sizeof(*d), 1);
  mod->data = d;
  d->frame = -666; // make sure we don't assume we already cached frame 0
  return 0;
}

void cleanup(dt_module_t *mod)
{
  vid_data_t *d = mod->data;
  decode_video_cleanup(&d->v);
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

  // if already cached filename, only init roi etc
  if(strncmp(d->filename, filename, sizeof(d->filename)))
  {
    if(d->filename[0]) decode_video_cleanup(&d->v);
    if(decode_video_init(&d->v, graph, filename))
    {
      dt_log(s_log_err, "[i-vid] could not initialise video!");
      return;
    }
  }

  parse_parameters(mod, d);
  mod->connector[0].roi.full_wd = d->v.wd;
  mod->connector[0].roi.full_ht = d->v.ht;
  float b = 0.0, w = 1.0f;
  const int prim = dt_module_param_int(mod, 0)[0];
  const int trc  = dt_module_param_int(mod, 1)[0];
  mod->img_param = (dt_image_params_t) {
    .black          = {b, b, b, b},
    .white          = {w, w, w, w},
    .whitebalance   = {1.0, 1.0, 1.0, 1.0},
    .filters        = 0, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, d->v.wd, d->v.ht},

    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},
    .orientation    = 0,
    // .exposure       = dat->video.EXPO.shutterValue * 1e-6f,
    // .aperture       = dat->video.LENS.aperture * 0.01f,
    // .iso            = dat->video.EXPO.isoValue,
    // .focal_length   = dat->video.LENS.focalLength,
    .colour_primaries = prim,
    .colour_trc       = trc,

    // .snd_samplerate = d->actx ? d->actx->sample_rate : 0,
    // .snd_format     = av_sample_fmt_to_alsa(d->actx ? d->actx->sample_fmt : -1),
    // .snd_channels   = d->actx ? d->actx->ch_layout.nb_channels : 0,

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  fs_createdate(filename, mod->img_param.datetime);
  int64_t frame_cnt = d->v.video.av_stream->nb_frames;
  if(!frame_cnt)
  {
    double time_base = av_q2d(d->v.video.av_stream->time_base);
    double duration = d->v.video.av_stream->duration * time_base; // in seconds
    frame_cnt = duration * d->v.fps;
  }
  // don't overwrite user settings:
  if(mod->graph->frame_cnt == 0)
    mod->graph->frame_cnt = frame_cnt;
  if(mod->graph->frame_rate == 0)
    mod->graph->frame_rate = d->v.fps;
  snprintf(d->filename, sizeof(d->filename), "%s", filename);
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 0 || parid == 1) // colour space
  {
    const int prim = dt_module_param_int(module, 0)[0];
    const int trc  = dt_module_param_int(module, 1)[0];
    module->img_param.colour_primaries = prim;
    module->img_param.colour_trc       = trc;
    // propagate image params and potentially different buffer sizes downwards (works with cmd update only)
  }
  return s_graph_run_record_cmd_buf;
}

void
commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  vid_data_t *d = module->data;
  int delta = graph->frame - d->frame;
  if(delta)
  {
    if(abs(delta) > 2) decode_seek(&d->v, graph->frame / d->v.fps);
    decode_video_graph_run_pre_node(&d->v, graph, graph->node+d->nid); // submit video decoding command buffer, if any
  }
  d->frame = graph->frame;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  vid_data_t *d = module->data;
  d->nid = dt_node_add(graph, module, "i-vid", "conv",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 1,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  graph->node[d->nid].type = s_node_vid_dec;
  graph->node[d->nid].connector[0].flags = s_conn_protected; // don't overwrite our old frames
  dt_connector_copy(graph, module, 0, d->nid, 1);
}
