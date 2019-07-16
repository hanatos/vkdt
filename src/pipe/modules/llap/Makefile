TARGET=libllap.so
CFLAGS=-Wall -I../.. -I../../.. -fPIC
CFLAGS+=$(OPT_CFLAGS)
LDFLAGS+=$(OPT_LDFLAGS) -lm

$(TARGET): main.c Makefile ../api.h ../../connector.c ../../connector.h
	$(CC) $(CFLAGS) main.c ../../connector.c -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

