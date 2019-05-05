// unfortunately we'll link to rawspeed, so we need c++ here.

// TODO: need some meta param struct that gives us the filename

// TODO: how do we get the vulkan buffers?
// assume the caller takes care of vkMapMemory etc
int load_input(
    void *mapped)
{
  // TODO: map source buffer
  uint16_t *buf = (uint16_t *)mapped;

  // TODO: use cpu cache for buffers, memcpy over:

    char filen[PATH_MAX] = { 0 };
  snprintf(filen, sizeof(filen), "%s", filename);
  FileReader f(filen);

  std::unique_ptr<RawDecoder> d;
  std::unique_ptr<const Buffer> m;

  try
  {
    dt_rawspeed_load_meta();

    m = f.readFile();

    RawParser t(m.get());
    d = t.getDecoder(meta);

    if(!d.get()) return DT_IMAGEIO_FILE_CORRUPTED;

    d->failOnUnknown = true;
    d->checkSupport(meta);
    d->decodeRaw();
    d->decodeMetaData(meta);
    RawImage r = d->mRaw;

    const auto errors = r->getErrors();
    for(const auto &error : errors) fprintf(stderr, "[rawspeed] (%s) %s\n", img->filename, error.c_str());

#if 0
    // g_strlcpy(img->camera_maker, r->metadata.canonical_make.c_str(), sizeof(img->camera_maker));
    // g_strlcpy(img->camera_model, r->metadata.canonical_model.c_str(), sizeof(img->camera_model));
    // g_strlcpy(img->camera_alias, r->metadata.canonical_alias.c_str(), sizeof(img->camera_alias));

    img->raw_black_level = r->blackLevel;
    img->raw_white_point = r->whitePoint;

    if(r->blackLevelSeparate[0] == -1 || r->blackLevelSeparate[1] == -1 || r->blackLevelSeparate[2] == -1
       || r->blackLevelSeparate[3] == -1)
    {
      r->calculateBlackAreas();
    }

    for(uint8_t i = 0; i < 4; i++) img->raw_black_level_separate[i] = r->blackLevelSeparate[i];

    if(r->blackLevel == -1)
    {
      float black = 0.0f;
      for(uint8_t i = 0; i < 4; i++)
      {
        black += img->raw_black_level_separate[i];
      }
      black /= 4.0f;

      img->raw_black_level = CLAMP(black, 0, UINT16_MAX);
    }
#endif

    /* free auto pointers on spot */
    d.reset();
    m.reset();

    // Grab the WB
    // for(int i = 0; i < 4; i++) img->wb_coeffs[i] = r->metadata.wbCoeffs[i];

    // img->buf_dsc.filters = 0u;
    // if(!r->isCFA && !dt_image_is_monochrome(img))
    // {
    //   dt_imageio_retval_t ret = dt_imageio_open_rawspeed_sraw(img, r, mbuf);
    //   return ret;
    // }

#if 0
    if((r->getDataType() != TYPE_USHORT16) && (r->getDataType() != TYPE_FLOAT32)) return DT_IMAGEIO_FILE_CORRUPTED;
    if((r->getBpp() != sizeof(uint16_t)) && (r->getBpp() != sizeof(float))) return DT_IMAGEIO_FILE_CORRUPTED;

    if((r->getDataType() == TYPE_USHORT16) && (r->getBpp() != sizeof(uint16_t))) return DT_IMAGEIO_FILE_CORRUPTED;

    if((r->getDataType() == TYPE_FLOAT32) && (r->getBpp() != sizeof(float))) return DT_IMAGEIO_FILE_CORRUPTED;

    const float cpp = r->getCpp();
    if(cpp != 1) return DT_IMAGEIO_FILE_CORRUPTED;

    img->buf_dsc.channels = 1;

    switch(r->getBpp())
    {
      case sizeof(uint16_t):
        img->buf_dsc.datatype = TYPE_UINT16;
        break;
      case sizeof(float):
        img->buf_dsc.datatype = TYPE_FLOAT;
        break;
      default:
        return DT_IMAGEIO_FILE_CORRUPTED;
    }

    // dimensions of uncropped image
    iPoint2D dimUncropped = r->getUncroppedDim();
    img->width = dimUncropped.x;
    img->height = dimUncropped.y;

    // dimensions of cropped image
    iPoint2D dimCropped = r->dim;

    // crop - Top,Left corner
    iPoint2D cropTL = r->getCropOffset();
    img->crop_x = cropTL.x;
    img->crop_y = cropTL.y;
#endif

    // load some more meta stuff.
    // TODO: may want that in some init phase during size negotiations

    // crop - Bottom,Right corner
    iPoint2D cropBR = dimUncropped - dimCropped - cropTL;
    img->crop_width = cropBR.x;
    img->crop_height = cropBR.y;

    img->fuji_rotation_pos = r->metadata.fujiRotationPos;
    img->pixel_aspect_ratio = (float)r->metadata.pixelAspectRatio;

    // as the X-Trans filters comments later on states, these are for
    // cropped image, so we need to uncrop them.
    img->buf_dsc.filters = dt_rawspeed_crop_dcraw_filters(r->cfa.getDcrawFilter(), cropTL.x, cropTL.y);

    if(FILTERS_ARE_4BAYER(img->buf_dsc.filters)) img->flags |= DT_IMAGE_4BAYER;

    if(img->buf_dsc.filters)
    {
      img->flags &= ~DT_IMAGE_LDR;
      img->flags |= DT_IMAGE_RAW;
      if(r->getDataType() == TYPE_FLOAT32)
      {
        img->flags |= DT_IMAGE_HDR;

        // we assume that image is normalized before.
        // FIXME: not true for hdrmerge DNG's.
        for(int k = 0; k < 4; k++) img->buf_dsc.processed_maximum[k] = 1.0f;
      }

      // special handling for x-trans sensors
      if(img->buf_dsc.filters == 9u)
      {
        // get 6x6 CFA offset from top left of cropped image
        // NOTE: This is different from how things are done with Bayer
        // sensors. For these, the CFA in cameras.xml is pre-offset
        // depending on the distance modulo 2 between raw and usable
        // image data. For X-Trans, the CFA in cameras.xml is
        // (currently) aligned with the top left of the raw data.
        for(int i = 0; i < 6; ++i)
          for(int j = 0; j < 6; ++j)
          {
            img->buf_dsc.xtrans[j][i] = r->cfa.getColorAt(i % 6, j % 6);
          }
      }
    }

     // if buf is NULL, we quit the fct here
    if(!mbuf) return DT_IMAGEIO_OK;

    // TODO: this would be the mapped buffer we get,
    // TODO: or some cpu side cache:
    void *buf = dt_mipmap_cache_alloc(mbuf, img);
    if(!buf) return DT_IMAGEIO_CACHE_FULL;

    /*
     * since we do not want to crop black borders at this stage,
     * and we do not want to rotate image, we can just use memcpy,
     * as it is faster than dt_imageio_flip_buffers, but only if
     * buffer sizes are equal,
     * (from Klaus: r->pitch may differ from DT pitch (line to line spacing))
     * else fallback to generic dt_imageio_flip_buffers()
     */
    const size_t bufSize_mipmap = (size_t)img->width * img->height * r->getBpp();
    const size_t bufSize_rawspeed = (size_t)r->pitch * dimUncropped.y;
    if(bufSize_mipmap == bufSize_rawspeed)
    {
      memcpy(buf, r->getDataUncropped(0, 0), bufSize_mipmap);
    }
    else
    {
      dt_imageio_flip_buffers((char *)buf, (char *)r->getDataUncropped(0, 0), r->getBpp(), dimUncropped.x,
                              dimUncropped.y, dimUncropped.x, dimUncropped.y, r->pitch, ORIENTATION_NONE);
    }
  }
  catch(const std::exception &exc)
  {
    printf("[rawspeed] (%s) %s\n", img->filename, exc.what());

    /* if an exception is raised lets not retry or handle the
     specific ones, consider the file as corrupted */
    return DT_IMAGEIO_FILE_CORRUPTED;
  }
  catch(...)
  {
    printf("Unhandled exception in imageio_rawspeed\n");
    return DT_IMAGEIO_FILE_CORRUPTED;
  }

  return DT_IMAGEIO_OK;
}

// TODO: callbacks for size negotiation
