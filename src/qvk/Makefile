TARGET=libqvk.a
TARGET_GUI=libqvkgui.a

.PHONY:clean

CC=clang
AR=ar
CFLAGS=-Wall -I. -I..
LDFLAGS=-lvulkan

$(TARGET):qvk.c qvk.h Makefile
	$(CC) $(CFLAGS) qvk.c -c -o qvk.o
	$(AR) r libqvk.a qvk.o

clean:
	rm -f $(TARGET) $(TARGET_GUI)
