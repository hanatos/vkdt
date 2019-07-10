TARGET=libhist.so
CFLAGS=-Wall -I../.. -I../../.. -fPIC
CFLAGS+=$(OPT_CFLAGS)
LDFLAGS+=$(OPT_LDFLAGS) -lm

$(TARGET): main.c Makefile
	$(CC) $(CFLAGS) main.c ../../connector.c -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

