#include "modules/api.h"
#include "core/fs.h"
#include "vid.h"
#include "vid-init.c"
#include "vid-run.c"

#include <stdint.h>

enum spa_audio_format_copy {
	SPA_AUDIO_FORMAT_UNKNOWN,
	SPA_AUDIO_FORMAT_ENCODED,

	/* interleaved formats */
	SPA_AUDIO_FORMAT_START_Interleaved	= 0x100,
	SPA_AUDIO_FORMAT_S8,
	SPA_AUDIO_FORMAT_U8,
	SPA_AUDIO_FORMAT_S16_LE,
	SPA_AUDIO_FORMAT_S16_BE,
	SPA_AUDIO_FORMAT_U16_LE,
	SPA_AUDIO_FORMAT_U16_BE,
	SPA_AUDIO_FORMAT_S24_32_LE,
	SPA_AUDIO_FORMAT_S24_32_BE,
	SPA_AUDIO_FORMAT_U24_32_LE,
	SPA_AUDIO_FORMAT_U24_32_BE,
	SPA_AUDIO_FORMAT_S32_LE,
	SPA_AUDIO_FORMAT_S32_BE,
	SPA_AUDIO_FORMAT_U32_LE,
	SPA_AUDIO_FORMAT_U32_BE,
	SPA_AUDIO_FORMAT_S24_LE,
	SPA_AUDIO_FORMAT_S24_BE,
	SPA_AUDIO_FORMAT_U24_LE,
	SPA_AUDIO_FORMAT_U24_BE,
	SPA_AUDIO_FORMAT_S20_LE,
	SPA_AUDIO_FORMAT_S20_BE,
	SPA_AUDIO_FORMAT_U20_LE,
	SPA_AUDIO_FORMAT_U20_BE,
	SPA_AUDIO_FORMAT_S18_LE,
	SPA_AUDIO_FORMAT_S18_BE,
	SPA_AUDIO_FORMAT_U18_LE,
	SPA_AUDIO_FORMAT_U18_BE,
	SPA_AUDIO_FORMAT_F32_LE,
	SPA_AUDIO_FORMAT_F32_BE,
	SPA_AUDIO_FORMAT_F64_LE,
	SPA_AUDIO_FORMAT_F64_BE,

	SPA_AUDIO_FORMAT_ULAW,
	SPA_AUDIO_FORMAT_ALAW,

	/* planar formats */
	SPA_AUDIO_FORMAT_START_Planar		= 0x200,
	SPA_AUDIO_FORMAT_U8P,
	SPA_AUDIO_FORMAT_S16P,
	SPA_AUDIO_FORMAT_S24_32P,
	SPA_AUDIO_FORMAT_S32P,
	SPA_AUDIO_FORMAT_S24P,
	SPA_AUDIO_FORMAT_F32P,
	SPA_AUDIO_FORMAT_F64P,
	SPA_AUDIO_FORMAT_S8P,
};

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

  // XXX this is not initialised for vulkan hardware devices:
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
  int format = 0;
  switch(d->v.format) {
    case AV_SAMPLE_FMT_U8:   format = SPA_AUDIO_FORMAT_U8;     break;
    case AV_SAMPLE_FMT_S16:  format = SPA_AUDIO_FORMAT_S16_LE; break;
    case AV_SAMPLE_FMT_S32:  format = SPA_AUDIO_FORMAT_S32_LE; break;
    case AV_SAMPLE_FMT_FLT:  format = SPA_AUDIO_FORMAT_F32_LE; break;
    case AV_SAMPLE_FMT_DBL:  format = SPA_AUDIO_FORMAT_F64_LE; break;
    case AV_SAMPLE_FMT_U8P:  format = SPA_AUDIO_FORMAT_U8P;    break;
    case AV_SAMPLE_FMT_S16P: format = SPA_AUDIO_FORMAT_S16P;   break;
    case AV_SAMPLE_FMT_S32P: format = SPA_AUDIO_FORMAT_S32P;   break;
    case AV_SAMPLE_FMT_FLTP: format = SPA_AUDIO_FORMAT_F32P;   break;
    case AV_SAMPLE_FMT_DBLP: format = SPA_AUDIO_FORMAT_F64P;   break;
    default: format = 0;                             
  }

  mod->connector[0].flags = s_conn_protected; // don't overwrite our old frames
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

    .snd_samplerate = d->v.sample_rate,
    .snd_format     = format,
    .snd_channels   = d->v.channels,

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  fs_createdate(filename, mod->img_param.datetime);
  int64_t frame_cnt = 0;//d->v.video.av_stream->nb_frames; // unreliable :(
  if(!frame_cnt)
  {
    double time_base = av_q2d(d->v.video.av_stream->time_base);
    double duration = d->v.video.av_stream->duration * time_base; // in seconds
    frame_cnt = duration * d->v.fps;
  }
  // don't overwrite user settings:
  if(mod->graph->frame_cnt <= 1) // 1 is the default
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
  // will be copied over from module. in fact setting it on the node is ineffective:
  // graph->node[d->nid].connector[0].flags = s_conn_protected; // don't overwrite our old frames
  dt_connector_copy(graph, module, 0, d->nid, 1);
}

uint32_t
audio(
    dt_module_t *module,
    void        *buf,
    uint32_t     size)
{
  vid_data_t *d = module->data;
  decode_video_t *v = &d->v;
  uint32_t res = 0;
  threads_mutex_lock(&v->av_mutex);
  // double pts_our = module->graph->frame / v->fps;
  // double time_base = av_q2d(v->audio.av_stream->time_base);
  while(res < size)
  {
    AVFrame *frame = v->arb.frame[v->arb.rdi];
    if(!frame) goto out;
    if(v->arb.rdi == v->arb.wri) goto out;
#if 0
    // FIXME: apparently sometimes there are video frames in the stream like mad and audio lags significantly.
    // TODO: also check video lag (timestamps vs graph frame). do we need some warmup phase?
    // buffer audio in full?
    double pts_snd = frame->pts * time_base;
    fprintf(stderr, "timestamp frame %g timestamp snd %g\n", pts_our, pts_snd);
    if(pts_our - 2.0/v->fps > pts_snd && v->arb.rdi < v->arb.wri-1) // FIXME some modulo logic broken
    {
      fprintf(stderr, "skipping audio ahead! %d -> %d\n", v->arb.rdi, v->arb.wri);
      goto next_frame;
    }
#endif

    int frame_size = v->audio_stride * frame->nb_samples;
    int new_res = res + frame_size - v->arb.rpos;
    new_res = MIN(size, new_res);
    int inc = new_res - res;
    memcpy(buf + res, frame->data[0] + v->arb.rpos, inc);
    res = new_res;
    v->arb.rpos += inc;

    if(v->arb.rpos >= frame_size)
    {
// next_frame:
      v->arb.rdi = (v->arb.rdi+1)%LENGTH(v->arb.frame);
      v->arb.rpos = 0;
    }
  }
out:
  threads_mutex_unlock(&v->av_mutex);
  return res;
}
