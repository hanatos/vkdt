MOD_CFLAGS=$(shell pkg-config --cflags libavformat --cflags libavcodec)
MOD_LDFLAGS=$(shell pkg-config --libs libavformat --libs libavcodec)
MOD_C=pipe/connector.c
