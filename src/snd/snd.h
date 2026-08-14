#pragma once
#include <stdint.h>

// wrapper for potential different backends

typedef struct dt_snd_t
{
  void *handle;
  int sample_rate;
  int channels;
  int format;
}
dt_snd_t;

void dt_snd_cleanup(dt_snd_t *snd);

int dt_snd_init(
    dt_snd_t *snd,
    int       sample_rate,
    int       channels,
    int       format);
