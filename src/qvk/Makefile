TARGET=libqvk.a
TARGET_GUI=libqvkgui.a

.PHONY:clean

CC=clang
AR=ar
CFLAGS=-Wall -I. -I..
LDFLAGS=-lvulkan
CFLAGS+=$(OPT_CFLAGS)
LDFLAGS+=$(OPT_LDFLAGS)

DEPS_O=qvk.o\
       qvk_util.o
DEPS_H=qvk.h\
       qvk_util.h

$(TARGET):$(DEPS_O) $(DEPS_H) Makefile
	$(AR) r libqvk.a $(DEPS_O)

clean:
	rm -f $(TARGET) $(TARGET_GUI) $(DEPS_O)
