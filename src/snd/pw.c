// pipewire audio output

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

typedef struct dt_snd_t
{
  int sample_rate; // e.g. 44100
  int channels;    // e.g. 2
  int format;      // e.g. SPA_AUDIO_FORMAT_S16
  int tid;
  struct pw_main_loop *loop;
  struct pw_stream *stream;
}
dt_snd_t;

static void on_process(void *data)
{
  dt_snd_t *snd = data;
  struct pw_buffer *b = pw_stream_dequeue_buffer(snd->stream);
  if(!b) return; // XXX ???
  struct spa_buffer *buf = b->buffer;
  if(!buf->datas[0].data) return; // ???
  uint32_t stride = sizeof(int16_t) * snd->channels;
  uint32_t frame_cnt = buf->datas[0].maxsize / stride;
  if(b->requested) frame_cnt = MIN(b->requested, frame_cnt);

  uint32_t size = dt_view_snd_process(buf, frame_cnt * stride);

  buf->datas[0].chunk->offset = 0;
  buf->datas[0].chunk->stride = stride;
  buf->datas[0].chunk->size   = frame_cnt * stride;
  pw_stream_queue_buffer(snd->stream, b);
}

static void
task_snd_work(uint32_t item, void *data)
{
  dt_snd_t *snd = data;
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
        .format   = snd->format,
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
  snd->sample_rate = sample_rate;
  snd->channels = channels;
  snd->format = format;
  snd->tid = threads_task("snd", 1, -1, snd, &task_snd_work, 0);
  return 0;
}

int dt_snd_cleanup(dt_snd_t *snd)
{
  if(snd->tid >= 0)
  {
    pw_main_loop_quit(snd->loop);
    threads_wait(snd->tid);
    snd->tid = -1;
  }
  return 0;
}
