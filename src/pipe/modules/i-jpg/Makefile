TARGET=lib$(shell basename $(CURDIR)).so
JPEG_I=
JPEG_L=-ljpeg
CFLAGS=-Wall -I../.. -I../../.. -fPIC -g $(JPEG_I)
LDFLAGS=$(JPEG_L)
CFLAGS+=$(OPT_CFLAGS)
LDFLAGS+=$(OPT_LDFLAGS)

$(TARGET): main.c Makefile
	$(CC) $(CFLAGS) main.c -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
