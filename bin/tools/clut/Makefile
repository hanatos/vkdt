CC=clang
CFLAGS=-D_XOPEN_SOURCE=600 -Wall -std=c11 -g  -D_POSIX_C_SOURCE=200809L
CFLAGS+=-march=native -O2
# LDFLAGS=-lm -llapack -lblas -lf2c -Llevmar-2.6/ -llevmar
# LDFLAGS=-lm -Llevmar-2.6/ -llevmar
LDFLAGS=-lm

# CFLAGS+=-fno-omit-frame-pointer -fsanitize=address
# LDFLAGS+=-fsanitize=address

all: fit

fit: main.c inpaint.h solve.h q2t.h clip.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf fit
