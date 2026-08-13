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

  enum AVColorPrimaries color_primaries = d->v.video.av_stream->codecpar->color_primaries;
  enum AVColorTransferCharacteristic color_trc = d->v.video.av_stream->codecpar->color_trc;
  if(p_colour[0] == -1)
  {
    p_colour[0] = s_colour_primaries_srgb; // default to bt.709
    if(color_primaries ==  1) p_colour[0] = s_colour_primaries_srgb; // bt.709
    if(color_primaries ==  9) p_colour[0] = s_colour_primaries_2020; // bt.2020 non constant luminance
    if(color_primaries == 10) p_colour[0] = s_colour_primaries_XYZ;  // cie xyz
    if(color_primaries == 12) p_colour[0] = s_colour_primaries_P3;   // display P3
  }

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
}

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
