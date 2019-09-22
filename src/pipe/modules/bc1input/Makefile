TARGET=libbc1input.so
CFLAGS=-Wall -I../.. -I../../.. -fPIC -g
LDFLAGS=-lz
CFLAGS+=$(OPT_CFLAGS)
LDFLAGS+=$(OPT_LDFLAGS)

$(TARGET): main.c Makefile
	$(CC) $(CFLAGS) main.c -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
