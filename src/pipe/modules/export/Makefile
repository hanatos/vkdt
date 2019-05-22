TARGET=libexport.so
CC=clang
CFLAGS=-Wall -I../.. -I../../.. -fPIC
CFLAGS+=-march=native -O3

$(TARGET): main.c Makefile
	$(CC) $(CFLAGS) main.c -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

