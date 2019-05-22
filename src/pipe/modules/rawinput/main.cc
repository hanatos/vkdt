// unfortunately we'll link to rawspeed, so we need c++ here.
#include "RawSpeed-API.h"

extern "C" {
#include "module.h"

static rawspeed::CameraMetaData *meta = 0;

typedef struct rawinput_buf_t
{
  std::unique_ptr<rawspeed::RawDecoder> d;
  std::unique_ptr<const rawspeed::Buffer> m;

  char filename[PATH_MAX] = {0};
}
rawinput_buf_t;

static void
rawspeed_load_meta()
{
  /* Load rawspeed cameras.xml meta file once */
  if(meta == NULL)
  {
    // XXX dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
    if(meta == NULL)
    {
      // char datadir[PATH_MAX] = { 0 };
      char camfile[PATH_MAX] = { 0 };
      // dt_loc_get_datadir(datadir, sizeof(datadir));
      const char *datadir = "../../ext/rawspeed/data"; // XXX
      snprintf(camfile, sizeof(camfile), "%s/cameras.xml", datadir);
      // never cleaned up (only when dt closes)
      meta = new rawspeed::CameraMetaData(camfile);
    }
    // XXX dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  }
}

static int
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
  // TODO: load image if not happened yet
  // int err = load_raw(mod, filename);
  load_raw(mod, "test.cr2");
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rawspeed::iPoint2D dim_uncropped = mod_data->d->mRaw->getUncroppedDim();
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
  int err = load_raw(mod, "test.cr2");
  if(err) return 1;
  uint16_t *buf = (uint16_t *)mapped;

  // dimensions of uncropped image
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rawspeed::iPoint2D dim_uncropped = mod_data->d->mRaw->getUncroppedDim();
  int wd = dim_uncropped.x;
  int ht = dim_uncropped.y;

  const size_t bufsize_compact = (size_t)wd * ht * mod_data->d->mRaw->getBpp();
  const size_t bufsize_rawspeed = (size_t)mod_data->d->mRaw->pitch * ht;
  if(bufsize_compact == bufsize_rawspeed)
  {
    memcpy(buf, mod_data->d->mRaw->getDataUncropped(0, 0), bufsize_compact);
    return 0;
  }
  else
  {
    fprintf(stderr, "[rawspeed] failed\n");
    return 1;
  }
}

} // extern "C"
