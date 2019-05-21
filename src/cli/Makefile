TARGET=vkdt-cli

.PHONY:clean

CC=clang
CFLAGS=-Wall -I. -I.. -g
OPTFLAGS=-march=native -O3
LDFLAGS=-L../qvk -lqvk -lvulkan

$(TARGET):main.c Makefile
	$(CC) $(CFLAGS) main.c -o vkdt-cli $(LDFLAGS)

clean:
	rm -f $(TARGET)
