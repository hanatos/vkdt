// unfortunately, since we'll link to rawspeed, we need c++ here.
#include "RawSpeed-API.h"
#include "mat3.h"
#include <omp.h>
#include <unistd.h>
#include <mutex>
#include <ctime>
#include <math.h>
#ifdef VKDT_USE_EXIV2
#include "exif.h"
#endif

extern "C" {
#include "modules/api.h"

static rawspeed::CameraMetaData *meta = 0;

int rawspeed_get_number_of_processor_cores()
{
  return sysconf(_SC_NPROCESSORS_ONLN);
}

typedef struct rawinput_buf_t
{
  std::unique_ptr<rawspeed::RawDecoder> d;
  std::unique_ptr<const rawspeed::Buffer> m;

  char filename[PATH_MAX] = {0};

  int ox, oy;
}
rawinput_buf_t;

namespace {

inline int
FC(const size_t row, const size_t col, const uint32_t filters)
{
  return filters >> (((row << 1 & 14) + (col & 1)) << 1) & 3;
}

inline int
FCxtrans(
    const int row, const int col,
    const uint8_t (*const xtrans)[6])
{
  return xtrans[row][col];
}


// take file name param and start frame param and return located raw file name.
// returns non-zero on failure.
int
get_filename(
    dt_module_t *mod,
    const char  *fname,
    int          frame,
    char        *ret,
    size_t       ret_size)
{
  char tmp[2*PATH_MAX+10];
  
  if(fname[0] != '/') // relative paths
  {
    snprintf(tmp, sizeof(tmp), "%s/%s", mod->graph->searchpath, fname);
    snprintf(ret, ret_size, tmp, frame);
    FILE *f = fopen(ret, "rb");
    if(!f)
    {
      snprintf(tmp, sizeof(tmp), "%s/%s", mod->graph->basedir, fname);
      snprintf(ret, ret_size, tmp, frame);
      f = fopen(tmp, "rb");
      if(!f) return 1; // damn that.
    }
    fclose(f);
  }
  else
  { // absolute path:
    snprintf(ret, ret_size, fname, frame);
    FILE *f = fopen(ret, "rb");
    if(!f) return 1;
    fclose(f);
  }
  return 0;
}

void
rawspeed_load_meta(const dt_module_t *mod)
{
  static std::mutex lock;
  if(meta == NULL)
  { /* Load rawspeed cameras.xml meta file once */
    lock.lock();
    if(meta == NULL)
    {
      omp_set_max_active_levels(5);
      char camfile[PATH_MAX+100];
      snprintf(camfile, sizeof(camfile), "%s/data/cameras.xml", mod->graph->basedir);
      try
      { // never cleaned up (only when dt closes)
        meta = new rawspeed::CameraMetaData(camfile);
      }
      catch(...)
      {
        fprintf(stderr, "[rawspeed] could not open cameras.xml!\n");
      }
    }
    lock.unlock();
  }
}

void
free_raw(dt_module_t *mod)
{ // free auto pointers
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  if(mod_data->d.get()) mod_data->d.reset();
  if(mod_data->m.get()) mod_data->m.reset();
}

int
load_raw(
    dt_module_t *mod,
    const char *filename)
{
  clock_t beg = clock();
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  if(mod_data)
  {
    if(!strcmp(mod_data->filename, filename))
      return 0; // already loaded
    else free_raw(mod); // maybe loaded the wrong one
  }
  else
  {
    assert(0); // this should be inited in init()
  }

  rawspeed::FileReader f(filename);

  try
  {
    rawspeed_load_meta(mod);

    mod_data->m = f.readFile();

    rawspeed::RawParser t(*mod_data->m);
    mod_data->d = t.getDecoder(meta);

    if(!mod_data->d.get()) return 1;

    mod_data->d->failOnUnknown = true;
    mod_data->d->checkSupport(meta);
    mod_data->d->decodeRaw();

    mod_data->d->decodeMetaData(meta);

    const auto errors = mod_data->d->mRaw->getErrors();
    for(const auto &error : errors) fprintf(stderr, "[rawspeed] (%s) %s\n", filename, error.c_str());

    // TODO: do some corruption detection and support for esoteric formats/fails here
    // the data type doesn't seem to be inited on hdrmerge raws:
    // if(mod_data->d->mRaw->getDataType() == rawspeed::TYPE_FLOAT32)
    if(sizeof(uint16_t) != mod_data->d->mRaw->getBpp())
    {
      fprintf(stderr, "[i-raw] unhandled pixel format: %s\n", filename);
      return 1;
    }
  }
  catch(const std::exception &exc)
  {
    printf("[rawspeed] (%s) %s\n", filename, exc.what());
    return 1;
  }
  catch(...)
  {
    printf("[rawspeed] unhandled exception in\n");
    return 1;
  }
  clock_t end = clock();
  snprintf(mod_data->filename, sizeof(mod_data->filename), "%s", filename);
  fprintf(stderr, "[rawspeed] load %s in %3.0fms\n", filename, 1000.0*(end-beg)/CLOCKS_PER_SEC);
  return 0;
}

} // end anonymous namespace

int init(dt_module_t *mod)
{
  rawinput_buf_t *dat = new rawinput_buf_t();
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
#if 0 // DEBUG: keep address sanitizer/leak checking happy:
  // remember to switch this off for non unit testing. it's
  // a performance nightmare and not thread safe either.
  delete meta;
  meta = 0;
#endif

  if(!mod->data) return;
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  free_raw(mod);
  delete mod_data;
  mod->data = 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if(parid == 1 || parid == 2) // noise model
  {
    const float noise_a = dt_module_param_float(module, 1)[0];
    const float noise_b = dt_module_param_float(module, 2)[0];
    module->img_param.noise_a = noise_a;
    module->img_param.noise_b = noise_b;
    return s_graph_run_all; // need no do modify_roi_out again to read noise model from file
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
  const int   id    = dt_module_param_int(mod, 3)[0];
  const char *fname = dt_module_param_string(mod, 0);
  char        filename[2*PATH_MAX+10];
  if(get_filename(mod, fname, id, filename, sizeof(filename))) return;

  if(strstr(fname, "%04d"))
  { // reading a sequence of raws as a timelapse animation
    mod->flags = s_module_request_read_source;
  }
  
  if(load_raw(mod, filename)) return;
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rawspeed::iPoint2D dim_uncropped = mod_data->d->mRaw->getUncroppedDim();
  // we know we only have one connector called "output" (see our "connectors" file)
  mod->connector[0].roi.full_wd = dim_uncropped.x;
  mod->connector[0].roi.full_ht = dim_uncropped.y;

  // TODO: data type, channels, bpp

  // TODO: put all the metadata nonsense in one csv ascii/binary file so we can
  // parse it more quickly.
  // put the real matrix in there directly, so we don't have to juggle
  // chromatic adaptation here.
  float *noise_a = (float*)dt_module_param_float(mod, 1);
  float *noise_b = (float*)dt_module_param_float(mod, 2);
  for(int k=0;k<9;k++)
  mod->img_param.cam_to_rec2020[k] = 0.0f/0.0f; // mark as uninitialised
#ifdef VKDT_USE_EXIV2 // now essentially only for exposure time/aperture value
  dt_exif_read(&mod->img_param, filename);
#endif
  // set a bit of metadata from rawspeed, overwrite exiv2 because this one is more consistent:
  snprintf(mod->img_param.maker, sizeof(mod->img_param.maker), "%s", mod_data->d->mRaw->metadata.canonical_make.c_str());
  snprintf(mod->img_param.model, sizeof(mod->img_param.model), "%s", mod_data->d->mRaw->metadata.canonical_model.c_str());
  mod->img_param.iso = mod_data->d->mRaw->metadata.isoSpeed;
  if(noise_a[0] == 0.0f && noise_b[0] == 0.0f)
  {
    char pname[512];
    snprintf(pname, sizeof(pname), "data/nprof/%s-%s-%d.nprof",
        mod->img_param.maker,
        mod->img_param.model,
        (int)mod->img_param.iso);
    FILE *f = dt_graph_open_resource(graph, pname, "rb");
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

  // dimensions of cropped image (cut away black borders for noise estimation)
  rawspeed::iPoint2D dimCropped = mod_data->d->mRaw->dim;
  rawspeed::iPoint2D cropTL = mod_data->d->mRaw->getCropOffset();
  mod->img_param.crop_aabb[0] = cropTL.x;
  mod->img_param.crop_aabb[1] = cropTL.y;
  mod->img_param.crop_aabb[2] = cropTL.x + dimCropped.x;
  mod->img_param.crop_aabb[3] = cropTL.y + dimCropped.y;

  if(mod_data->d->mRaw->blackLevelSeparate[0] == -1)
    mod_data->d->mRaw->calculateBlackAreas();
  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = mod_data->d->mRaw->blackLevelSeparate[k];
    mod->img_param.white[k]        = mod_data->d->mRaw->whitePoint;
    mod->img_param.whitebalance[k] = mod_data->d->mRaw->metadata.wbCoeffs[k];
  }
  // normalise wb
  mod->img_param.whitebalance[0] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[2] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[3] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[1] = 1.0f;

  if(isnanf(mod->img_param.cam_to_rec2020[0]))
  { // camera matrix not found in exif or compiled without exiv2
    float xyz_to_cam[12], mat[9] = {0};
    if(mod_data->d->mRaw->metadata.colorMatrix.size() > 0)
    { // get d65 camera matrix from rawspeed
      for(int k=0;k<9;k++)
        xyz_to_cam[k] = mod_data->d->mRaw->metadata.colorMatrix[k] / 10000.0f;
      mat3inv(mat, xyz_to_cam);
    }
    else mat[0] = mat[4] = mat[8] = 1.0;

    // compute matrix camrgb -> rec2020 d65
    double cam_to_xyz[] = {
      mat[0], mat[1], mat[2],
      mat[3], mat[4], mat[5],
      mat[6], mat[7], mat[8]};

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

  // uncrop bayer sensor filter
  mod->img_param.filters = mod_data->d->mRaw->cfa.getDcrawFilter();
  if(mod->img_param.filters != 9u)
    mod->img_param.filters = rawspeed::ColorFilterArray::shiftDcrawFilter(
        mod_data->d->mRaw->cfa.getDcrawFilter(),
        cropTL.x, cropTL.y);

  // now we need to account for the pixel shift due to an offset filter:
  dt_roi_t *ro = &mod->connector[0].roi;
  int ox = 0, oy = 0;

  // special handling for x-trans sensors
  if(mod->img_param.filters == 9u)
  {
    uint8_t f[6][6];
    // get 6x6 CFA offset from top left of cropped image
    // NOTE: This is different from how things are done with Bayer
    // sensors. For these, the CFA in cameras.xml is pre-offset
    // depending on the distance modulo 2 between raw and usable
    // image data. For X-Trans, the CFA in cameras.xml is
    // (currently) aligned with the top left of the raw data.
    for(int i = 0; i < 6; ++i)
      for(int j = 0; j < 6; ++j)
        f[j][i] = static_cast<uint8_t>(mod_data->d->mRaw->cfa.getColorAt(i, j));

    // find first green in same row
    for(ox=0;ox<6&&FCxtrans(0,ox,f)!=1;ox++)
      ;
    if(FCxtrans(0,ox+1,f) != 1 && FCxtrans(0,ox+2,f) != 1) // center of x-cross, need to go 2 down
    {
      oy = 2;
      ox = (ox + 2) % 3;
    }
    if(FCxtrans(oy+1,ox,f) == 1) // two greens above one another, nede to go down one
      oy++;
    if(FCxtrans(oy,ox+1,f) == 1)
    { // if x+1 is green, too, either x++ or x-=2 if x>=2
      if(ox >= 2) ox -= 2;
      else ox++;
    }
    // now we should be at the beginning of a green 5-cross block.
    if(FCxtrans(oy,ox+1,f) == 2)
    { // if x+1 == red and y+1 == blue, all good!
      // if not, x+=3 or y+=3, equivalently.
      if(ox < oy) ox += 3;
      else        oy += 3;
    }
  }
  else // move to std bayer sensor offset
  {
    uint32_t f = mod->img_param.filters;
    if(FC(0,0,f) == 1)
    {
      if(FC(0,1,f) == 0) ox = 1;
      if(FC(0,1,f) == 2) oy = 1;
    }
    else if(FC(0,0,f) == 2)
    {
      ox = oy = 1;
    }
  }
  // unfortunately need to round to full 6x6 block for xtrans
  const int block = mod->img_param.filters == 9u ? 3 : 2;
  const int bigblock = mod->img_param.filters == 9u ? 6 : 2;
  // crop aabb is relative to buffer we emit,
  // so we need to move it along to the CFA pattern boundary
  uint32_t *b = mod->img_param.crop_aabb;
  b[2] -= ox;
  b[3] -= oy;
  // fprintf(stderr, "off %d %d\n", ox, oy);
  // fprintf(stderr, "box %u %u %u %u\n", b[0], b[1], b[2], b[3]);
  // and also we'll round it to cut only between CFA blocks
  b[0] = ((b[0] + bigblock - 1) / bigblock) * bigblock;
  b[1] = ((b[1] + bigblock - 1) / bigblock) * bigblock;
  b[2] =  (b[2] / block) * block;
  b[3] =  (b[3] / block) * block;
  // fprintf(stderr, "bor %u %u %u %u\n", b[0], b[1], b[2], b[3]);

  mod_data->ox = ox;
  mod_data->oy = oy;
  ro->full_wd -= ox;
  ro->full_ht -= oy;
  // round down to full block size:
  ro->full_wd = (ro->full_wd/block)*block;
  ro->full_ht = (ro->full_ht/block)*block;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const int   id    = dt_module_param_int(mod, 3)[0];
  const char *fname = dt_module_param_string(mod, 0);
  char        filename[2*PATH_MAX+10];
  if(get_filename(mod, fname, id + mod->graph->frame, filename, sizeof(filename)))
    return 1;
  int err = load_raw(mod, filename);
  if(err) return 1;
  uint16_t *buf = (uint16_t *)mapped;

  // dimensions of uncropped image
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rawspeed::iPoint2D dim_uncropped = mod_data->d->mRaw->getUncroppedDim();
  int wd = dim_uncropped.x;
  int ht = dim_uncropped.y;

  int ox = mod_data->ox;
  int oy = mod_data->oy;
  wd -= ox;
  ht -= oy;
  // round down to full block size:
  const int block = mod->img_param.filters == 9u ? 3 : 2;
  wd = (wd/block)*block;
  ht = (ht/block)*block;
  // TODO: make sure the roi we get on the connector agrees with this!
  const size_t bufsize_compact = (size_t)wd * ht * sizeof(uint16_t); // mod_data->d->mRaw->getBpp();
  const size_t bufsize_rawspeed = (size_t)mod_data->d->mRaw->pitch * dim_uncropped.y;
  if(bufsize_compact == bufsize_rawspeed)
  {
    memcpy(buf, mod_data->d->mRaw->getDataUncropped(0, 0), bufsize_compact);
    return 0;
  }
  else
  {
    for(int j=0;j<ht;j++)
      memcpy(buf + j*wd, mod_data->d->mRaw->getDataUncropped(ox, j+oy), sizeof(uint16_t)*wd);
    return 0;
  }
}

} // extern "C"
