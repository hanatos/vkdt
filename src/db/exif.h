#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// fake exif extraction unit. so far we extract the exif tag 0x9004 CreateDate (or similar)
// admittedly the matching is a bit fuzzy, as you will observe:
static inline int
dt_db_exif_mini(
    const char *filename,
    char       *createdate,  // size is always 20
    char       *model,
    size_t      model_size)
{
  if(model_size) model[0] = 0;
  createdate[0] = 0;
  FILE *f = fopen(filename, "rb");
  if(!f)
  {
    snprintf(createdate, 20, "1970:01:01 00:00:00");
    return 1;
  }

  char buf[512];

  fread(buf, sizeof(buf), 1, f);
  fclose(f);

  // scan for maker model as follows:
  // SONY..ILCE-7M3
  // FUJIFILM..X100F (not at position 0)
  // Canon.Canon MODEL (search for "Canon ")
  // NIKON CORPORATION...NIKON MODEL (discard the one with corporation)

  int i = 4;
  if(!strncmp(buf, "MOTION ", 7)) // catch mcraw
    i = snprintf(model, MIN(7,model_size), "%s", buf);
  for(;i<sizeof(buf)-20;i++)
  { // check maker/model
    if(!strncmp(buf+i, "SONY", 4))
      i += 1 + snprintf(model, model_size, "Sony %s", buf+i+6);
    else if(!strncmp(buf+i, "FUJIFILM", 8))
      i += 1 + snprintf(model, model_size, "Fujifilm %s", buf+i+10);
    else if(!strncmp(buf+i, "Canon", 5))
      i += 6 + snprintf(model, model_size, "%s", buf+i+6);
    else if(!strncmp(buf+i, "NIKON CORP", 10))
      i += 18;
    else if(!strncmp(buf+i, "NIKON ", 6))
      i += snprintf(model, model_size, "%s", buf+i);
    else if(!strncmp(buf+i, "Xiaomi", 6))
      i += snprintf(model, model_size, "%s", buf+i);
    else
    { // check for date string:
      if(buf[i+4] != ':') continue;
      if(buf[i+7] != ':') continue;
      if(buf[i+13] != ':') continue;
      if(buf[i+16] != ':') continue;
      if(buf[i+10] != ' ') continue;

#define NNUM(b) (buf[i+b] < '0' || buf[i+b] > '9')
      if(NNUM(0)) continue;
      if(NNUM(1)) continue;
      if(NNUM(2)) continue;
      if(NNUM(3)) continue;

      if(NNUM(5)) continue;
      if(NNUM(6)) continue;

      if(NNUM(8)) continue;
      if(NNUM(9)) continue;
      
      if(NNUM(11)) continue;
      if(NNUM(12)) continue;

      if(NNUM(14)) continue;
      if(NNUM(15)) continue;

      if(NNUM(17)) continue;
      if(NNUM(18)) continue;
#undef NNUM

      memcpy(createdate, buf+i, 19);
      createdate[19] = 0;
      // fprintf(stderr, "create date %s model %s\n", createdate, model);
      return 0;
    }
  }
  fs_createdate(filename, createdate);
  return 1;
}

