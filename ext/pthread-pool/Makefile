CFLAGS=-Wall -Werror -fPIC
OPTFLAGS=-Os -march=native
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %c,%o,$(SOURCES))

.PHONY: all clean

all:libpthreadpool.a

clean:
	rm -f *.o libpthreadpool.a

%.o: %.c *.h Makefile
	$(CC) $(CFLAGS) $(OPTFLAGS) -c $< -o $@

libpthreadpool.a: $(OBJECTS) Makefile
	$(AR) rc libpthreadpool.a $(OBJECTS)
