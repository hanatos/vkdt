#pragma once

typedef struct dt_snd_alsa_t
{
  void *handle;
  int sample_rate;
  int channels;
}
dt_snd_alsa_t;

void dt_snd_alsa_cleanup(dt_snd_alsa_t *snd);
int dt_snd_alsa_init(
    dt_snd_alsa_t *snd,
    int            sample_rate,
    int            channels);
int dt_snd_alsa_play(
    dt_snd_alsa_t *snd,
    uint16_t      *samples,
    int            sample_cnt);


// wrappers for potential different backends
typedef struct dt_snd_alsa_t dt_snd_t;

void dt_snd_cleanup(dt_snd_t *snd)
{
  dt_snd_alsa_cleanup(snd);
}

int dt_snd_init(
    dt_snd_t *snd,
    int       sample_rate,
    int       channels)
{
  return dt_snd_alsa_init(snd, sample_rate, channels);
}

int dt_snd_play(
    dt_snd_t *snd,
    uint16_t *samples,
    int       sample_cnt)
{
  return dt_snd_alsa_play(snd, samples, sample_cnt);
}
