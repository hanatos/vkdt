SND_H=snd/snd.h
SND_CFLAGS=
ifeq ($(VKDT_USE_ALSA), 1)
  SND_LDFLAGS=$(shell pkg-config --libs alsa)
  SND_CFLAGS=$(shell pkg-config --cflags alsa)
  SND_O=snd/alsa.o
else
  SND_LDFLAGS=
  SND_O=snd/none.o
endif
