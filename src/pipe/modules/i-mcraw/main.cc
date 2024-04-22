#include "modules/api.h"
#include "connector.h"
#include "core/core.h"

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
  int32_t wd, ht;
  std::vector<uint16_t> dummy; // tmp mem to decode frame, please remove this for direct access to mapped memory!
};

int
open_file(
    dt_module_t *mod,
    const char  *fname)
{
  buf_t *dat = (buf_t *)mod->data;
  if(dat && !strcmp(dat->filename, fname))
    return 0; // already open

  fprintf(stderr, "[i-mcraw] opening `%s'\n", fname);

  char filename[512];
  if(dt_graph_get_resource_filename(mod, fname, 0, filename, sizeof(filename)))
    return 1; // file not found

  dat->dec = new motioncam::Decoder(filename);

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

  try {
    dat->dec->loadAudio(dat->audio_chunks);
  } catch(...) {
    return 1;
  }

  snprintf(dat->filename, sizeof(dat->filename), "%s", fname);
  return 0;
}

int
read_frame(
    dt_module_t *mod,
    void        *mapped)
{
  buf_t *dat = (buf_t *)mod->data;
  const std::vector<motioncam::Timestamp> &frame_list = dat->dec->getFrames();
  int frame = CLAMP(mod->graph->frame, 0, frame_list.size()-1);

  // this is so dumb, we could decode directly into gpu memory, avoiding 2 memcopies!
  nlohmann::json metadata;
  try {
    dat->dec->loadFrame(frame_list[frame], dat->dummy, metadata);
  } catch(...) {
    return 1;
  }

  uint32_t wd = metadata["width"];
  uint32_t ht = metadata["height"];

  if(dat->wd != wd || dat->ht != ht) return 1;
  memcpy(mapped, dat->dummy.data(), wd*ht*sizeof(uint16_t));

  return 0;
}

int init(dt_module_t *mod)
{
  buf_t *dat = new buf_t();
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  mod->flags = s_module_request_read_source;
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
  delete dat; // destroy audio std vector too
  mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return;
  buf_t *dat= (buf_t *)mod->data;
  const uint32_t wd = dat->wd;
  const uint32_t ht = dat->ht;
  mod->connector[0].roi.full_wd = wd;
  mod->connector[0].roi.full_ht = ht;

  const nlohmann::json& cmeta = dat->dec->getContainerMetadata();
  // TODO somehow wire this to params per frame (commit_params?)
  // std::vector<double> wb = dat->metadata["asShotNeutral"];
  std::vector<uint16_t> black = cmeta["blackLevel"];
  float white = cmeta["whiteLevel"];
  std::string sensorArrangement = cmeta["sensorArrangment"];
  std::vector<double> colorMatrix1 = cmeta["colorMatrix1"];
  std::vector<double> colorMatrix2 = cmeta["colorMatrix2"];
  std::vector<double> fwd = cmeta["forwardMatrix1"];
  std::vector<double> forwardMatrix2 = cmeta["forwardMatrix2"];

  mod->img_param = (dt_image_params_t) {
    .black          = {(float)black[0], (float)black[1], (float)black[2], (float)black[3]}, // XXX do we always get 4?
    .white          = {white, white, white, white},
    // .whitebalance   = {wb[0], wb[1], wb[2], wb[3]},
    .whitebalance   = {1, 1, 1, 1},
    .filters        = 0x5d5d5d5d, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, wd, ht},
    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},

    .orientation    = 0,
  // ???  .exposure       = dat->video.EXPO.shutterValue * 1e-6f,
  // ???  .aperture       = dat->video.LENS.aperture * 0.01f,
  // ???  .iso            = dat->video.EXPO.isoValue,
  // ???  .focal_length   = dat->video.LENS.focalLength,
    .snd_format     = 2, // ==SND_PCM_FORMAT_S16_LE,
    .snd_channels   = dat->dec->numAudioChannels(),
    .snd_samplerate = dat->dec->audioSampleRateHz(),

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  // snprintf(mod->img_param.datetime, sizeof(mod->img_param.datetime), "%4d%2d%2d %2d:%2d%2d",
  //     dat->video.RTCI.tm_year, dat->video.RTCI.tm_mon, dat->video.RTCI.tm_mday,
  //     dat->video.RTCI.tm_hour, dat->video.RTCI.tm_min, dat->video.RTCI.tm_sec);
  // snprintf(mod->img_param.model, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  // snprintf(mod->img_param.maker, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  // for(int i=0;i<sizeof(mod->img_param.maker);i++) if(mod->img_param.maker[i] == ' ') mod->img_param.maker[i] = 0;
  mod->graph->frame_cnt  = dat->dec->getFrames().size();
  // TODO mod->graph->frame_rate = dat->video.frame_rate;

  // load noise profile:
  char pname[512];
  snprintf(pname, sizeof(pname), "data/nprof/%s-%s-%d.nprof",
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
      mod->img_param.noise_a = a;
      mod->img_param.noise_b = b;
    }
    fclose(f);
  }
  
  // compute matrix camrgb -> rec2020 d65
  double cam_to_xyz[] = {
    fwd[0], fwd[1], fwd[2],
    fwd[3], fwd[4], fwd[5],
    fwd[6], fwd[7], fwd[8]};
  // find white balance baked into matrix:
  mod->img_param.whitebalance[0] = (cam_to_xyz[0]+cam_to_xyz[1]+cam_to_xyz[2]);
  mod->img_param.whitebalance[1] = (cam_to_xyz[3]+cam_to_xyz[4]+cam_to_xyz[5]);
  mod->img_param.whitebalance[2] = (cam_to_xyz[6]+cam_to_xyz[7]+cam_to_xyz[8]);
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
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return 1;
  return read_frame(mod, mapped);
}

#if 0
int audio(
    dt_module_t  *mod,
    const int     frame,
    uint8_t     **samples)
{
  buf_t *dat = mod->data;
  if(!dat || !dat->filename[0])
    return 0;


  int channels = dat->dec->numAudioChannels();
  int rate = dat->dec->audioSampleRateHz();

  // TODO:
  // find right audio chunk for frame by timestamp, i.e. audio_chunk.first
  // *samples = audio_chunk.second
  // return audio_chuck.second / channels; // number of samples
  // TODO: are these timestamps always the right granularity for the frame?
}
#endif
} // extern "C"
