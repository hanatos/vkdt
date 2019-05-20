// unfortunately we'll link to rawspeed, so we need c++ here.
#include "RawSpeed-API.h"

extern "C" {

static CameraMetaData *meta = 0;

typedef struct rawinput_buf_t
{
  RawImage rimg;
  char filename[PATH_MAX] = {0};
}
rawinput_buf_t;

static void
load_rawspeed_meta()
{
  /* Load rawspeed cameras.xml meta file once */
  if(meta == NULL)
  {
    // XXX dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
    if(meta == NULL)
    {
      char datadir[PATH_MAX] = { 0 }, camfile[PATH_MAX] = { 0 };
      // dt_loc_get_datadir(datadir, sizeof(datadir));
      datadir[0] = '.'; // XXX
      snprintf(camfile, sizeof(camfile), "%s/rawspeed/cameras.xml", datadir);
      // never cleaned up (only when dt closes)
      meta = new CameraMetaData(camfile);
    }
    // XXX dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  }
}

static int
load_raw(
    dt_module_t *mod,
    const char *filename)
{
  dt_rawinput_buf_t *mod_data = mod->data;
  if(mod_data)
  {
    if(!strcmp(mod_data->filename, filename))
      return 0; // already loaded
  }

  snprintf(mod_data->filename, sizeof(mod_data->filename), "%s", mod_data->filename);
  FileReader f(mod_data->filename);

  std::unique_ptr<RawDecoder> d;
  std::unique_ptr<const Buffer> m;

  try
  {
    dt_rawspeed_load_meta();

    m = f.readFile();

    RawParser t(m.get());
    d = t.getDecoder(meta);

    if(!d.get()) return 1;

    d->failOnUnknown = true;
    d->checkSupport(meta);
    d->decodeRaw();
    d->decodeMetaData(meta);
    mod_data->rimg = d->mRaw;

    const auto errors = mod_data->rimg->getErrors();
    for(const auto &error : errors) fprintf(stderr, "[rawspeed] (%s) %s\n", mod_data->filename, error.c_str());

    /* free auto pointers */
    d.reset();
    m.reset();

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

void init(dt_module_t *mod)
{
  dt_rawinput_buf_t *dat = new dt_rawinput_buf_t();
}

void cleanup(dt_module_t *mod)
{
  dt_rawinput_buf_t *mod_data = mod->data;
  delete mod_data; // deletes RawImage
  mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_module_t *mod)
{
  // TODO: load image if not happened yet
  // int err = load_raw(mod, filename);
  dt_rawinput_buf_t *mod_data = mod->data;
  iPoint2D dim_uncropped = mod_data->rimg->getUncroppedDim();
  // we know we only have one connector called "output" (see our "connectors" file)
  mod->connector[0].roi.full_wd = dim_uncropped.x;
  mod->connector[0].roi.full_ht = dim_uncropped.y;
}

// TODO: how do we get the vulkan buffers?
// assume the caller takes care of vkMapMemory etc
int load_input(
    dt_module_t *mod,
    void *mapped)
{
  // XXX
  // int err = load_raw(mod, filename);
  uint16_t *buf = mapped;

  // dimensions of uncropped image
  dt_rawinput_buf_t *mod_data = mod->data;
  iPoint2D dim_uncropped = mod_data->rimg->getUncroppedDim();
  int wd = dim_uncropped.x;
  int ht = dim_uncropped.y;

  const size_t bufsize_compact = (size_t)wd * ht * mod_data->rimg->getBpp();
  const size_t bufsize_rawspeed = (size_t)mod_data->rimg->pitch * ht;
  if(bufsize_mipmap == bufsize_rawspeed)
  {
    memcpy(buf, mod_data->rimg->getDataUncropped(0, 0), bufsize_compact);
    return 0;
  }
  else
  {
    fprintf(stderr, "[rawspeed] failed\n");
    return 1;
  }
}

} // extern "C"
