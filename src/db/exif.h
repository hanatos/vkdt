#pragma once

// fake exif extraction unit. so far we extract the exif tag 0x9004 CreateDate (or similar)
// admittedly the matching is a bit fuzzy, as you will observe:
static inline int
dt_db_exif_createdate(
    const char *filename,
    char       *createdate)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;

  char buf[512];

  fread(buf, sizeof(buf), 1, f);
  fclose(f);

  for(int i=0;i<sizeof(buf)-20;i++)
  {
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
    // fprintf(stderr, "create date %s\n", createdate);
    return 0;
  }
  return 1;
}

