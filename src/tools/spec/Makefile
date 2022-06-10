LDFLAGS=-lm
CFLAGS=-I../.. -I.. -fopenmp -march=native -O3

mkabney: createlut.c Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

macadam: macadam.c Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
