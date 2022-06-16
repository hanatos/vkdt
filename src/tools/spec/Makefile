LDFLAGS=-lm
CFLAGS=-I../.. -I.. -fopenmp -march=native -O3
.PHONY:luts

luts:../../../bin/data/spectra.lut

../../../bin/data/spectra.lut: mkabney macadam.lut Makefile
	./mkabney
	mv spectra.lut abney.lut ../../../bin/data/

macadam.lut: macadam
	./macadam

mkabney: createlut.c Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

macadam: macadam.c Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
