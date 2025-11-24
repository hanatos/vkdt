extern "C" {
#include "modules/api.h"
#include "connector.h"
#include "core/core.h"
// #include "core/log.h"
#include "core/mat3.h"
#include "pipe/dng_opcode.h"
}

#include "motioncam/Decoder.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

extern "C" {

struct buf_t
{
  char filename[256];
  motioncam::Decoder *dec;
  std::vector<motioncam::AudioChunk> audio_chunks;
  int32_t wd, ht, rwd; // original image dimensions from decoder
  int ox, oy;          // offset to first canonical rggb bayer block
  float as_shot_neutral[4];
  std::vector<uint16_t> dummy; // tmp mem to decode frame, please remove this for direct access to mapped memory!
  uint16_t *bitcnt;
  dt_image_metadata_dngop_t dngop;
  dt_dng_opcode_list_t     *oplist;
  dt_dng_gain_map_t        *gainmap[4];
};

#if 0
commit_params()
{
  // grab the following from metadata? most doesn't seem to change, luckily (gainmap for instance)
  // ,"asShotNeutral":[0.51171875,1.0,0.578125],"needRemosaic":false,"iso":234,"exposureCompensation":0,"exposureTime":10000000.0,"orientation":0,"isCompressed":true,"compressionType":7,"dynamicWhiteLevel":1023.0,"dynamicBlackLevel":[63.9375,64.0,63.9375,64.0],
}
#endif

int
open_file(
    dt_module_t *mod,
    const char  *fname)
{
  buf_t *dat = (buf_t *)mod->data;
  if(dat && !strcmp(dat->filename, fname))
    return 0; // already open

  char filename[512];
  if(dt_graph_get_resource_filename(mod, fname, 0, filename, sizeof(filename)))
    return 1; // file not found

  try {
    dat->dec = new motioncam::Decoder(filename);
  } catch(...) {
    return 1;
  }

  // load first frame to find out about resolution
  const std::vector<motioncam::Timestamp> &frame_list = dat->dec->getFrames();
  if(frame_list.size() == 0) return 1; // no frames in file
  std::vector<uint16_t> dummy;
  nlohmann::json metadata;

  try {
    dat->dec->loadFrame(frame_list[0], dummy, metadata);
  } catch(...) {
    return 1;
  }

  dat->wd = metadata["width"];
  dat->ht = metadata["height"];
  std::vector<double> wb = metadata["asShotNeutral"];
  for(int k=0;k<3;k++) dat->as_shot_neutral[k] = 1.0/wb[k];

  // load gainmap  
  int uncropped_wd = metadata["originalWidth"];
  int uncropped_ht = metadata["originalHeight"];
  int shwd = metadata["lensShadingMapWidth"];
  int shht = metadata["lensShadingMapHeight"];
  // if this is not always present, check via .at() and catch basic_json::out_of_range exception instead of []:
  std::vector<std::vector<double> > shmp = metadata["lensShadingMap"]; // is [4][wd*ht]
  dat->dngop.type = s_image_metadata_dngop;
  dat->dngop.op_list[0] = 0;
  dat->dngop.op_list[1] = 0;
  dat->dngop.op_list[2] = 0;

  dat->oplist = (dt_dng_opcode_list_t *)malloc(sizeof(dt_dng_opcode_list_t) + sizeof(dt_dng_opcode_t)*4);
  for(int k=0;k<4;k++)
  {
    dat->gainmap[k] = (dt_dng_gain_map_t *)malloc(sizeof(dt_dng_gain_map_t) + sizeof(float)*shwd*shht);
    dat->gainmap[k]->map_points_h  = metadata["lensShadingMapWidth"];
    dat->gainmap[k]->map_points_v  = metadata["lensShadingMapHeight"];
    dat->gainmap[k]->map_spacing_h = uncropped_wd / (shwd-1.0);
    dat->gainmap[k]->map_spacing_v = uncropped_ht / (shht-1.0);
    dat->gainmap[k]->map_origin_v  = 0.5f;
    dat->gainmap[k]->map_origin_h  = 0.5f;
    dat->gainmap[k]->map_planes    = 1;
    for(int j=0;j<shht;j++) for(int i=0;i<shwd;i++)
      dat->gainmap[k]->map_gain[shwd * j + i] = shmp[k][shwd*j+i];

    dat->gainmap[k]->region.plane = 0;
    dat->gainmap[k]->region.planes = 1;
    dat->gainmap[k]->region.row_pitch = 2;
    dat->gainmap[k]->region.col_pitch = 2;
    dat->gainmap[k]->region.top =  k >> 1;
    dat->gainmap[k]->region.left = k & 1;

    dat->oplist->ops[k].id = s_dngop_gain_map;
    dat->oplist->ops[k].optional = 1;
    dat->oplist->ops[k].preview_skip = 0;
    dat->oplist->ops[k].data = dat->gainmap[k];
  }
  dat->oplist->count = 4;
  dat->dngop.op_list[1] = dat->oplist;

  dat->rwd = (((dat->wd + 63)/64) * 64); // round up to multiple of 64 for the decoder
  const uint32_t blocks = dat->rwd * dat->ht / 64;
  const uint32_t rblock = ((blocks+63)/64) * 64;
  if(dat->bitcnt) free(dat->bitcnt);
  dat->bitcnt = (uint16_t*)malloc(sizeof(uint16_t)*2*((rblock+1)/2));

  try {
    dat->dec->loadAudio(dat->audio_chunks);
  } catch(...) {
    return 1;
  }

  snprintf(dat->filename, sizeof(dat->filename), "%s", fname);
  return 0;
}

int init(dt_module_t *mod)
{
  buf_t *dat = new buf_t();
  memset(dat->filename, 0, sizeof(dat->filename));
  dat->bitcnt = 0;
  mod->data = dat;
  mod->flags = s_module_request_read_source;
  dat->gainmap[0] = dat->gainmap[1] = dat->gainmap[2] = dat->gainmap[3] = 0;
  dat->oplist = 0;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  buf_t *dat= (buf_t *)mod->data;
  if(dat->filename[0])
  {
    delete dat->dec;
    dat->filename[0] = 0;
  }
  if(dat->bitcnt) free(dat->bitcnt);
  for(int k=0;k<4;k++)
  {
    free(dat->gainmap[k]);
    dat->gainmap[k] = 0;
  }
  free(dat->oplist);  dat->oplist  = 0; 
  delete dat; // destroy audio std vector too
  mod->data = 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     pid,
    uint32_t     num,
    void        *oldval)
{
  buf_t *dat = (buf_t *)module->data;
  int pid_start = dt_module_get_param(module->so, dt_token("start"));
  if(pid == pid_start)
  {
    int mxcnt = dat->dec->getFrames().size();
    int endfr = MIN(mxcnt, module->graph->frame_cnt + *((int*)oldval));
    int start = CLAMP(dt_module_param_int(module, pid)[0], 0, mxcnt-1);
    if(module->graph->frame_cnt > 1) // keep single frame extracts single
      module->graph->frame_cnt = MAX(1, MIN(endfr, mxcnt)-start);
    return s_graph_run_record_cmd_buf;
  }
  if(pid == 1 || pid == 2) // noise model
  {
    const float noise_a = dt_module_param_float(module, 1)[0];
    const float noise_b = dt_module_param_float(module, 2)[0];
    module->img_param.noise_a = noise_a;
    module->img_param.noise_b = noise_b;
    if(noise_a == 0.0 && noise_b == 0.0)
      return s_graph_run_all; // need to do modify_roi_out again to read noise model from file
  }
  return s_graph_run_record_cmd_buf;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return; // return on error (not if already loaded)
  buf_t *dat= (buf_t *)mod->data;
  const uint32_t wd = dat->wd;
  const uint32_t ht = dat->ht;
  dt_roi_t *ro = &mod->connector[0].roi;
  ro->full_wd = wd;
  ro->full_ht = ht;

  const nlohmann::json& cmeta = dat->dec->getContainerMetadata();
  std::vector<uint16_t> black = cmeta["blackLevel"];
  float white = cmeta["whiteLevel"];
  std::string sensorArrangement = cmeta["sensorArrangment"];
  std::vector<double> cm1 = cmeta["colorMatrix1"];
  // std::vector<double> colorMatrix2 = cmeta["colorMatrix2"];
  // std::vector<double> fwd = cmeta["forwardMatrix1"];
  // std::vector<double> forwardMatrix2 = cmeta["forwardMatrix2"];

  mod->img_param = (dt_image_params_t) {
    .black          = {(float)black[0], (float)black[1], (float)black[2], (float)black[3]}, // XXX do we always get 4?
    .white          = {white, white, white, white},
    .whitebalance   = {dat->as_shot_neutral[0], dat->as_shot_neutral[1], dat->as_shot_neutral[2], dat->as_shot_neutral[3]},
    // .whitebalance   = {0, 0, 0, 0},
    .filters        = 0x5d5d5d5d, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, wd, ht},
    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},

    .orientation    = 0,
    .exposure       = 1.0f, // can't seem to get useful values, even per frame
    .aperture       = cmeta["apertures"][0],
    .iso            = 100.0f, // this is per frame really
    .focal_length   = cmeta["focalLengths"][0],
    .snd_format     = 2, // ==SND_PCM_FORMAT_S16_LE,
    .snd_channels   = dat->dec->numAudioChannels(),
    .snd_samplerate = dat->dec->audioSampleRateHz(),

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  // snprintf(mod->img_param.datetime, sizeof(mod->img_param.datetime), "%4d%2d%2d %2d:%2d%2d",
  //     dat->video.RTCI.tm_year, dat->video.RTCI.tm_mon, dat->video.RTCI.tm_mday,
  //     dat->video.RTCI.tm_hour, dat->video.RTCI.tm_min, dat->video.RTCI.tm_sec);
  snprintf(mod->img_param.model, sizeof(mod->img_param.model), "%s", cmeta["extraData"]["postProcessSettings"]["metadata"]["build.model"].template get<std::string>().c_str());
  snprintf(mod->img_param.maker, sizeof(mod->img_param.maker), "%s", cmeta["extraData"]["postProcessSettings"]["metadata"]["build.manufacturer"].template get<std::string>().c_str());
  // for(int i=0;i<sizeof(mod->img_param.maker);i++) if(mod->img_param.maker[i] == ' ') mod->img_param.maker[i] = 0;
  if(mod->graph->frame_cnt == 0)
    mod->graph->frame_cnt = dat->dec->getFrames().size();
  else
    mod->graph->frame_cnt = MIN(mod->graph->frame_cnt, dat->dec->getFrames().size());
  if(mod->graph->frame_rate < 1)
  { // estimate frame rate only if it's set to nothing reasonable
    if(mod->graph->frame_cnt > 5)
    {
      const int N = 5;
      double sum = 0.0;
      for(int i=1;i<N;i++)
        sum += dat->dec->getFrames()[i] - dat->dec->getFrames()[i-1];
      double avg = sum / (N-1.0);
      mod->graph->frame_rate = 1e9/avg;
    }
  }
  if(mod->graph->frame_rate < 1)
    mod->graph->frame_rate = 24; // okay whatever failed above let's assume 24

  // could probably check bool cmeta["deviceSpecificProfile"]["disableShadingMap"], only what does it mean?
  // append our gainmap/dngops if any
  if(dat->oplist)
    mod->img_param.meta = dt_metadata_append(mod->img_param.meta, (dt_image_metadata_t *)&dat->dngop);

  // load noise profile:
  float *noise_a = (float*)dt_module_param_float(mod, 1);
  float *noise_b = (float*)dt_module_param_float(mod, 2);
  if(noise_a[0] == 0.0f && noise_b[0] == 0.0f)
  {
    char pname[512];
    snprintf(pname, sizeof(pname), "nprof/%s-%s-%d.nprof",
        mod->img_param.maker,
        mod->img_param.model,
        (int)mod->img_param.iso);
    FILE *f = dt_graph_open_resource(graph, 0, pname, "rb");
    if(f)
    {
      float a = 0.0f, b = 0.0f;
      int num = fscanf(f, "%g %g", &a, &b);
      if(num == 2)
      {
        noise_a[0] = mod->img_param.noise_a = a;
        noise_b[0] = mod->img_param.noise_b = b;
      }
      fclose(f);
    }
  }
  else
  {
    mod->img_param.noise_a = noise_a[0];
    mod->img_param.noise_b = noise_b[0];
  }
  
  // compute matrix camrgb -> rec2020 d65
  float xyz_to_cam[9], mat[9] = {0};
  // get d65 camera matrix
  for(int i=0;i<9;i++)
    xyz_to_cam[i] = cm1[i];
  mat3inv(mat, xyz_to_cam);

  // compute matrix camrgb -> rec2020 d65
  double cam_to_xyz[] = {
    mat[0], mat[1], mat[2],
    mat[3], mat[4], mat[5],
    mat[6], mat[7], mat[8]};
  mod->img_param.whitebalance[0] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[2] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[1]  = 1.0f;

  const float xyz_to_rec2020[] = {
     1.7166511880, -0.3556707838, -0.2533662814,
    -0.6666843518,  1.6164812366,  0.0157685458,
     0.0176398574, -0.0427706133,  0.9421031212
  };
  float cam_to_rec2020[9] = {0.0f};
  for(int j=0;j<3;j++) for(int i=0;i<3;i++) for(int k=0;k<3;k++)
    cam_to_rec2020[3*j+i] +=
      xyz_to_rec2020[3*j+k] * cam_to_xyz[3*k+i];
  for(int k=0;k<9;k++)
    mod->img_param.cam_to_rec2020[k] = cam_to_rec2020[k];

  // deal with CFA offsets to rggb block:
  int ox = 0, oy = 0;
  if(sensorArrangement == "bggr") ox = oy = 1;
  if(sensorArrangement == "grbg") ox = 1;
  if(sensorArrangement == "gbrg") oy = 1;
  const int block = 2;
  const int bigblock = 2;
  // crop aabb is relative to buffer we emit,
  // so we need to move it along to the CFA pattern boundary
  uint32_t *b = mod->img_param.crop_aabb;
  b[2] -= ox;
  b[3] -= oy;
  // and also we'll round it to cut only between CFA blocks
  b[0] = ((b[0] + bigblock - 1) / bigblock) * bigblock;
  b[1] = ((b[1] + bigblock - 1) / bigblock) * bigblock;
  b[2] =  (b[2] / block) * block;
  b[3] =  (b[3] / block) * block;
  ro->full_wd -= ox;
  ro->full_ht -= oy;
  // round down to full block size:
  ro->full_wd = (ro->full_wd/block)*block;
  ro->full_ht = (ro->full_ht/block)*block;
  
  dat->ox = ox;
  dat->oy = oy;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  buf_t *dat = (buf_t *)module->data;
  const uint32_t data_size = sizeof(uint16_t)*dat->wd*dat->ht; // maximum size of encoded data blocks, for allocation
  const uint32_t blocks = dat->rwd * dat->ht / 64;             // number of decoding blocks/64 pixels each
  const uint32_t rblock = ((blocks+63)/64) * 64;               // buffer sizes rounded up to multiple of decoding block too
  dt_roi_t roi_block = {.wd = rblock, .ht = 1 };               // one per block, this is for reference values and data offsets
  dt_roi_t roi_bits  = {.wd = (rblock+1)/2, .ht = 1 };         // bits are encoded as 4 bits each, i.e. 2 per 8-bit pixel
  dt_roi_t roi_data  = {.wd = data_size, .ht = 1 };
  dt_roi_t roi_scrt  = {.wd = (rblock+511)/512+1, .ht = 4 };  // scratch size per partition, is workgroup size * number of rows

  // source nodes for data upload:
  const int id_bitcnt = dt_node_add(graph, module, "i-mcraw", "bitcnt", 1, 1, 1, 0, 0, 1,
    "bitcnt", "source", "ssbo", "ui8",  &roi_bits);
  const int id_refval = dt_node_add(graph, module, "i-mcraw", "refval", 1, 1, 1, 0, 0, 1,
    "refval", "source", "ssbo", "ui16", &roi_block);
  const int id_datain = dt_node_add(graph, module, "i-mcraw", "datain", 1, 1, 1, 0, 0, 1,
    "datain", "source", "ssbo", "ui8",  &roi_data);

  // prefix sum to compute byte offsets from bit counts per block:
  // runs at workgroupsize of 512
  const int id_prefix = dt_node_add(graph, module, "i-mcraw", "prefix", (blocks+511)/512 * DT_LOCAL_SIZE_X, 1, 1, 0, 0, 3,
    "bitcnt",  "read",  "ssbo", "ui8",   dt_no_roi,
    "offset",  "write", "ssbo", "ui32", &roi_block,
    "scratch", "write", "ssbo", "ui32", &roi_scrt);
  graph->node[id_prefix].connector[2].flags = static_cast<dt_connector_flags_t>(graph->node[id_prefix].connector[2].flags | s_conn_clear);
  // create decoder kernel:
  const int pc[] = { dat->ox, dat->oy, dat->rwd, dat->ht };
  const int id_decode = dt_node_add(graph, module, "i-mcraw", "decode", blocks * DT_LOCAL_SIZE_X, 1, 1, sizeof(pc), pc, 5,
    "output", "write", "rggb", "ui16", &module->connector[0].roi,
    "bitcnt", "read",  "ssbo", "ui8",  dt_no_roi,
    "offset", "read",  "ssbo", "ui32", dt_no_roi,
    "refval", "read",  "ssbo", "ui16", dt_no_roi,
    "datain", "read",  "ssbo", "ui8",  dt_no_roi);
  CONN(dt_node_connect_named(graph, id_bitcnt, "bitcnt", id_prefix, "bitcnt"));
  CONN(dt_node_connect_named(graph, id_bitcnt, "bitcnt", id_decode, "bitcnt"));
  CONN(dt_node_connect_named(graph, id_prefix, "offset", id_decode, "offset"));
  CONN(dt_node_connect_named(graph, id_refval, "refval", id_decode, "refval"));
  CONN(dt_node_connect_named(graph, id_datain, "datain", id_decode, "datain"));
  dt_connector_copy(graph, module, 0, id_decode, 0); // wire decoded output to module output
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  // double end, beg = dt_time();
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return 1;

  buf_t *dat = (buf_t *)mod->data;
  const int blocks = dat->rwd * dat->ht / 64;
  const int rblock = ((blocks+63)/64)*64;
  const std::vector<motioncam::Timestamp> &frame_list = dat->dec->getFrames();
  int pid_start = dt_module_get_param(mod->so, dt_token("start"));
  int start = MAX(dt_module_param_int(mod, pid_start)[0], 0);
  int frame = CLAMP(mod->graph->frame+start, (int)0, (int)(frame_list.size()-1));
  size_t out_data_max_len = sizeof(uint16_t) * rblock * 64;

  if(p->node->kernel == dt_token("bitcnt"))
  {
    dat->dec->getEncoded(frame_list[frame], dat->bitcnt, rblock, 0, 0, 0, 0);
    const int hb = (blocks+1)/2;
    uint8_t *out = (uint8_t *)mapped;
    for(int k=0;k<hb;k++)
      out[k] = CLAMP(dat->bitcnt[2*k+0], 0, 0xf) | (CLAMP(dat->bitcnt[2*k+1], 0, 0xf) << 4);
  }
  else if(p->node->kernel == dt_token("refval"))
  {
    dat->dec->getEncoded(frame_list[frame], 0, 0, (uint16_t*)mapped, rblock, 0, 0);
  }
  else if(p->node->kernel == dt_token("datain"))
  {
    dat->dec->getEncoded(frame_list[frame], 0, 0, 0, 0, (uint8_t*)mapped, out_data_max_len);
  }
  // end = dt_time();
  // dt_log(s_log_perf, "[i-mcraw] load %s %" PRItkn " in %3.2fms", dat->filename, dt_token_str(p->node->kernel), 1000.0*(end-beg));
  // fprintf(stderr, "[i-mcraw] load %s %" PRItkn " in %3.2fms\n", dat->filename, dt_token_str(p->node->kernel), 1000.0*(end-beg));
  return 0;
}

int audio(
    dt_module_t  *mod,
    uint64_t      sample_beg,
    uint32_t      sample_cnt,
    uint8_t     **samples)
{
  buf_t *dat = (buf_t *)mod->data;
  if(!dat || !dat->filename[0]) return 0;

  // guess chunk offset by start frame
  int pid_start = dt_module_get_param(mod->so, dt_token("start"));
  int start = MAX(dt_module_param_int(mod, pid_start)[0], 0);
  int mxcnt = dat->dec->getFrames().size();
  float t = start / (float)mxcnt;

  int channels   = dat->dec->numAudioChannels();
  int chunk_size = dat->audio_chunks[0].second.size() / channels; // is in number of samples, but already vec<int16>
  sample_beg += t * chunk_size * dat->audio_chunks.size();
  int chunk_id   = CLAMP((int)(sample_beg / chunk_size), (int)0, (int)(dat->audio_chunks.size()-1));
  int chunk_off  = CLAMP((int)(sample_beg - chunk_id * chunk_size), (int)0, (int)(chunk_size-1));
  // find right audio chunk for frame by timestamp, i.e. audio_chunk.first (so far they seem to be always zero?)
  *samples = (uint8_t*)(dat->audio_chunks[chunk_id].second.data() + chunk_off);

  return MIN((int)sample_cnt, (int)(chunk_size - chunk_off));
}
} // extern "C"
