// pipewire audio output
#include "core/log.h"
#include "gui/view.h"

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

typedef struct dt_pw_t
{
  int sample_rate; // e.g. 44100
  int channels;    // e.g. 2
  int format;      // e.g. SPA_AUDIO_FORMAT_S16
  int stride;      // sizeof(sample) * channels
  int tid;
  struct pw_main_loop *loop;
  struct pw_stream *stream;
}
dt_pw_t;

static void on_process(void *data)
{
  dt_pw_t *snd = data;
  struct pw_buffer *b = pw_stream_dequeue_buffer(snd->stream);
  if(!b) return; // XXX ???
  struct spa_buffer *buf = b->buffer;
  if(!buf->datas[0].data) return; // ???
  uint32_t stride = snd->stride;
  uint32_t frame_cnt = buf->datas[0].maxsize / stride;
  if(b->requested) frame_cnt = MIN(b->requested, frame_cnt);

  // TODO pass pointers for planes!
  // TODO planar formats?
  uint32_t size = dt_view_snd_process(buf->datas[0].data, frame_cnt * stride);

  buf->datas[0].chunk->offset = 0;
  buf->datas[0].chunk->stride = stride;
  buf->datas[0].chunk->size   = size;
  pw_stream_queue_buffer(snd->stream, b);
}

static void
task_snd_work(uint32_t item, void *data)
{
  dt_pw_t *snd = data;
  const struct spa_pod *params[1];
  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

  const struct pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = on_process,
  };
  snd->loop = pw_main_loop_new(NULL);

  snd->stream = pw_stream_new_simple(
      pw_main_loop_get_loop(snd->loop),
      "audio-src",
      pw_properties_new(
        PW_KEY_MEDIA_TYPE,     "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE,     "Music",
        NULL),
      &stream_events,
      data);

  params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
      &SPA_AUDIO_INFO_RAW_INIT(
        .format   = snd->format, // like SPA_AUDIO_FORMAT_S16
        .channels = snd->channels,
        .rate     = snd->sample_rate));

  pw_stream_connect(snd->stream,
      PW_DIRECTION_OUTPUT,
      PW_ID_ANY,
      PW_STREAM_FLAG_AUTOCONNECT |
      PW_STREAM_FLAG_MAP_BUFFERS |
      PW_STREAM_FLAG_RT_PROCESS,
      params, 1);

  pw_main_loop_run(snd->loop); // blocks until pw_main_loop_quit

  pw_stream_destroy(snd->stream);
  pw_main_loop_destroy(snd->loop);
  snd->tid = -1;
}

int dt_snd_init(
    dt_snd_t *snd,
    int       sample_rate,
    int       channels,
    int       format)
{
  pw_init(0, 0);
  memset(snd, 0, sizeof(dt_snd_t));
  dt_pw_t *pw = malloc(sizeof(*pw));
  snd->handle = pw;
  pw->sample_rate = snd->sample_rate = sample_rate;
  pw->channels = snd->channels = channels;
  pw->format = snd->format = format;
  int size = 0;
  switch(pw->format) {
    case SPA_AUDIO_FORMAT_U8:     size = 1; dt_log(s_log_pipe, "pw audio u8");  break;
    case SPA_AUDIO_FORMAT_S16_LE: size = 2; dt_log(s_log_pipe, "pw audio s16"); break;
    case SPA_AUDIO_FORMAT_S32_LE: size = 4; dt_log(s_log_pipe, "pw audio s32"); break;
    case SPA_AUDIO_FORMAT_F32_LE: size = 4; dt_log(s_log_pipe, "pw audio f32"); break;
    case SPA_AUDIO_FORMAT_F64_LE: size = 8; dt_log(s_log_pipe, "pw audio f64"); break;
    // FIXME planar formats won't make it through now:
    // case SPA_AUDIO_FORMAT_U8P:    size = 1; break;
    // case SPA_AUDIO_FORMAT_S16P:   size = 2; break;
    // case SPA_AUDIO_FORMAT_S32P:   size = 4; break;
    // case SPA_AUDIO_FORMAT_F32P:   size = 4; break;
    // case SPA_AUDIO_FORMAT_F64P:   size = 8; break;
    default: size = 0;
  }
  if(size == 0)
  {
    fprintf(stderr, "XXX unsupported audio format!\n");
    return 0; // unsupported sample format
  }
  pw->stride = channels * size;
  fprintf(stderr, "XXX starting pipewire task!\n");
  pw->tid = threads_task("snd", 1, -1, pw, &task_snd_work, 0);
  return 0;
}

void dt_snd_cleanup(dt_snd_t *snd)
{
  dt_pw_t *pw = snd->handle;
  if(!pw) return;
  if(pw->tid >= 0)
  {
    pw_main_loop_quit(pw->loop);
    threads_wait(pw->tid);
    pw->tid = -1;
  }
  free(snd->handle);
  memset(snd, 0, sizeof(dt_snd_t));
}
