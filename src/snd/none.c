#include "snd/snd.h"

// no sound

void dt_snd_cleanup(dt_snd_t *snd) { }

int dt_snd_init(
    dt_snd_t *snd,
    int       sample_rate,
    int       channels,
    int       format)
{
  return 0;
}

int dt_snd_play(
    dt_snd_t *snd,
    uint16_t *samples,
    int       sample_cnt)
{
  return 0;
}
