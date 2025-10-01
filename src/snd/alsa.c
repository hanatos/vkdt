#include "core/log.h"
#include "db/rc.h"
#include "gui/gui.h"
#include "snd/snd.h"

#include <stdint.h>
#include <alsa/asoundlib.h>

static void
dt_snd_alsa_cleanup(dt_snd_t *snd)
{
  snd_pcm_t *pcm = snd->handle;
  if(pcm) snd_pcm_close(pcm);
  snd->handle = 0;
}

static int
dt_snd_alsa_init(
    dt_snd_t *snd,
    int       sample_rate,
    int       channels,
    int       format)
{
  dt_snd_alsa_cleanup(snd); // just in case
  memset(snd, 0, sizeof(*snd));
  snd->sample_rate = sample_rate;
  snd->channels = channels;
  int err = 0;
  snd_pcm_hw_params_t *hwparams = 0;
  snd_pcm_sw_params_t *swparams = 0;

  snd_pcm_t *pcm = 0;

  const char *pcm_device = dt_rc_get(&vkdt.rc, "snd/alsa/pcm", "default");
  if(format == -1) goto error;
  if(0 > (err = snd_pcm_open(&pcm, pcm_device, SND_PCM_STREAM_PLAYBACK, 0)))
    goto error;

  snd->handle = pcm;

  if(0 > (err = snd_pcm_hw_params_malloc(&hwparams))) goto error;
  if(0 > (err = snd_pcm_hw_params_any(pcm, hwparams))) goto error;
  if(0 > (err = snd_pcm_hw_params_set_access(pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED))) goto error;
  if(0 > (err = snd_pcm_hw_params_set_format(pcm, hwparams, format))) goto error;
  if(0 > (err = snd_pcm_hw_params_set_rate(pcm, hwparams, sample_rate, 0))) goto error;
  if(0 > (err = snd_pcm_hw_params_set_channels(pcm, hwparams, channels))) goto error;
  if(0 > (err = snd_pcm_hw_params(pcm, hwparams))) goto error;

  if(0 > (err = snd_pcm_sw_params_malloc(&swparams))) goto error;
  if(0 > (err = snd_pcm_sw_params_current(pcm, swparams))) goto error;
  if(0 > (err = snd_pcm_sw_params_set_avail_min(pcm, swparams, 1024))) goto error;
  if(0 > (err = snd_pcm_sw_params_set_start_threshold(pcm, swparams, 0u))) goto error;
  if(0 > (err = snd_pcm_sw_params(pcm, swparams))) goto error;

  if(0 > (err = snd_pcm_prepare(pcm)))
    goto error;

  snd_pcm_hw_params_free(hwparams);
  snd_pcm_sw_params_free(swparams);
  dt_log(s_log_snd, "inited alsa device %s", pcm_device);
  return 0;

error:
  if(!err)
  {
    dt_log(s_log_err|s_log_snd, "alsa device had no format given! %d", format);
    return 1;
  }
  dt_log(s_log_err|s_log_snd, "failed to open alsa device %s: %d %s", pcm_device, err, snd_strerror(err));
  if(hwparams) snd_pcm_hw_params_free(hwparams);
  if(swparams) snd_pcm_sw_params_free(swparams);
  dt_snd_alsa_cleanup(snd);
  return err;
}

static int
dt_snd_alsa_play(
    dt_snd_t *snd,
    uint16_t *samples,
    int       sample_cnt)
{
  snd_pcm_t *pcm = snd->handle;
  if(!pcm) return 0;
  int err = snd_pcm_writei(pcm, samples, sample_cnt);
  if(err == -EAGAIN) return 0;
  if(err < 0) snd_pcm_prepare(pcm);
  return err > 0 ? err : 0;
}

void dt_snd_cleanup(dt_snd_t *snd)
{
  dt_snd_alsa_cleanup(snd);
}

int dt_snd_init(
    dt_snd_t *snd,
    int       sample_rate,
    int       channels,
    int       format)
{
  return dt_snd_alsa_init(snd, sample_rate, channels, format);
}

int dt_snd_play(
    dt_snd_t *snd,
    uint16_t *samples,
    int       sample_cnt)
{
  return dt_snd_alsa_play(snd, samples, sample_cnt);
}
