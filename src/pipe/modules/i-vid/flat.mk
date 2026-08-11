ifeq ($(VKDT_USE_FFMPEG),1)
MOD_CFLAGS=$(VKDT_AV_CFLAGS)
MOD_LDFLAGS=$(VKDT_AV_LDFLAGS)
endif
pipe/modules/i-vid/libi-vid.so: pipe/modules/i-vid/vid.h pipe/modules/i-vid/vid-init.c pipe/modules/i-vid/vid-run.c
