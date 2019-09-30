TARGET=libbc1out.so
CFLAGS=-Wall -I../.. -I../../.. -fPIC -g
LDFLAGS=-lz

$(TARGET): main.c Makefile
	$(CC) $(CFLAGS) main.c -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
