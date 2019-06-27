// unfortunately we'll link to rawspeed, so we need c++ here.
#include "RawSpeed-API.h"
#include <unistd.h>
#include <mutex>

extern "C" {
#include "modules/api.h"
#include "module.h"

static rawspeed::CameraMetaData *meta = 0;

// way to define a clean api:
// define this function, it is only declared in rawspeed:
int rawspeed_get_number_of_processor_cores()
{
  return sysconf(_SC_NPROCESSORS_ONLN);
}

typedef struct rawinput_buf_t
{
  std::unique_ptr<rawspeed::RawDecoder> d;
  std::unique_ptr<const rawspeed::Buffer> m;

  char filename[PATH_MAX] = {0};
}
rawinput_buf_t;

namespace {

// TODO: put in header!
inline int
FC(const size_t row, const size_t col, const uint32_t filters)
{
  return filters >> (((row << 1 & 14) + (col & 1)) << 1) & 3;
}

#if 0
inline int
FCxtrans(
    const int row, const int col, const dt_iop_roi_t *const roi,
    const uint8_t (*const xtrans)[6])
{
  // Add +600 (which must be a multiple of CFA width 6) as offset can
  // be negative and need to ensure a non-negative array index. The
  // negative offsets in current code come from the demosaic iop:
  // Markesteijn 1-pass (-12), Markesteijn 3-pass (-17), and VNG (-2).
  int irow = row + 600;
  int icol = col + 600;
  assert(irow >= 0 && icol >= 0);

  if(roi)
  {
    irow += roi->y;
    icol += roi->x;
  }

  return xtrans[irow % 6][icol % 6];
}
#endif
void
rawspeed_load_meta()
{
  static std::mutex lock;
  /* Load rawspeed cameras.xml meta file once */
  if(meta == NULL)
  {
    lock.lock();
    if(meta == NULL)
    {
      // char datadir[PATH_MAX] = { 0 };
      char camfile[PATH_MAX] = { 0 };
      // dt_loc_get_datadir(datadir, sizeof(datadir));
      const char *datadir = "./data"; // assume we run from installed bin/ directory
      snprintf(camfile, sizeof(camfile), "%s/cameras.xml", datadir);
      // never cleaned up (only when dt closes)
      try
      {
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

int
load_raw(
    dt_module_t *mod,
    const char *filename)
{
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  if(mod_data)
  {
    if(!strcmp(mod_data->filename, filename))
      return 0; // already loaded
  }
  else
  {
    assert(0); // this should be inited in init()
  }

  snprintf(mod_data->filename, sizeof(mod_data->filename), "%s", filename);
  rawspeed::FileReader f(mod_data->filename);

  try
  {
    rawspeed_load_meta();

    mod_data->m = f.readFile();

    rawspeed::RawParser t(mod_data->m.get());
    mod_data->d = t.getDecoder(meta);

    if(!mod_data->d.get()) return 1;

    mod_data->d->failOnUnknown = true;
    mod_data->d->checkSupport(meta);
    mod_data->d->decodeRaw();
    mod_data->d->decodeMetaData(meta);

    const auto errors = mod_data->d->mRaw->getErrors();
    for(const auto &error : errors) fprintf(stderr, "[rawspeed] (%s) %s\n", mod_data->filename, error.c_str());

    // TODO: do some corruption detection and support for esoteric formats/fails here
  }
  catch(const std::exception &exc)
  {
    printf("[rawspeed] (%s) %s\n", mod_data->filename, exc.what());
    return 1;
  }
  catch(...)
  {
    printf("[rawspeed] unhandled exception in\n");
    return 1;
  }
  return 0;
}

} // end anonymous namespace

int init(dt_module_t *mod)
{
  rawinput_buf_t *dat = new rawinput_buf_t();
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
#if 1 // DEBUG: keep address sanitizer/leak checking happy:
  // remember to switch this off for non unit testing. it's
  // a performance nightmare and not thread safe either.
  delete meta;
  meta = 0;
#endif

  if(!mod->data) return;
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  /* free auto pointers */
  mod_data->d.reset();
  mod_data->m.reset();
  delete mod_data;
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
  if(load_raw(mod, filename)) return;
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rawspeed::iPoint2D dim_uncropped = mod_data->d->mRaw->getUncroppedDim();
  // we know we only have one connector called "output" (see our "connectors" file)
  mod->connector[0].roi.full_wd = dim_uncropped.x;
  mod->connector[0].roi.full_ht = dim_uncropped.y;

  // TODO: data type, channels, bpp
  if(mod_data->d->mRaw->blackLevelSeparate[0] == -1)
    mod_data->d->mRaw->calculateBlackAreas();
  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = mod_data->d->mRaw->blackLevelSeparate[k];
    mod->img_param.white[k]        = mod_data->d->mRaw->whitePoint;
    mod->img_param.whitebalance[k] = mod_data->d->mRaw->metadata.wbCoeffs[k] * 1.0f/1024.0f;
  }
  // TODO: xtrans
  // uncrop bayer sensor filter
  rawspeed::iPoint2D cropTL = mod_data->d->mRaw->getCropOffset();
  mod->img_param.filters = rawspeed::ColorFilterArray::shiftDcrawFilter(
      mod_data->d->mRaw->cfa.getDcrawFilter(),
      cropTL.x, cropTL.y);

  // now we need to account for the pixel shift due to an offset filter:
  dt_roi_t *ro = &mod->connector[0].roi;
  uint32_t f = mod->img_param.filters;
  int ox = 0, oy = 0;
  if(FC(0,0,f) == 1)
  {
    if(FC(0,1,f) == 0) ox = 1;
    if(FC(0,1,f) == 2) oy = 1;
  }
  else if(FC(0,0,f) == 2)
  {
    ox = oy = 1;
  }
  ro->full_wd -= ox;
  ro->full_ht -= oy;
}

int read_source(
    dt_module_t *mod,
    void *mapped)
{
  const char *filename = dt_module_param_string(mod, 0);
  int err = load_raw(mod, filename);
  if(err) return 1;
  uint16_t *buf = (uint16_t *)mapped;

  // dimensions of uncropped image
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rawspeed::iPoint2D dim_uncropped = mod_data->d->mRaw->getUncroppedDim();
  int wd = dim_uncropped.x;
  int ht = dim_uncropped.y;

  uint32_t f = mod->img_param.filters;
  int ox = 0, oy = 0;
  if(FC(0,0,f) == 1)
  {
    if(FC(0,1,f) == 0) ox = 1;
    if(FC(0,1,f) == 2) oy = 1;
  }
  else if(FC(0,0,f) == 2)
  {
    ox = oy = 1;
  }
  wd -= ox;
  ht -= oy;
  const size_t bufsize_compact = (size_t)wd * ht * mod_data->d->mRaw->getBpp();
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
