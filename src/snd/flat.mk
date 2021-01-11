SND_H=snd/snd.h
SND_CFLAGS=
ifeq ($(VKDT_USE_ALSA), 1)
  SND_LDFLAGS=-lasound
  SND_O=snd/alsa.o
else
  SND_LDFLAGS=
  SND_O=snd/none.o
endif
