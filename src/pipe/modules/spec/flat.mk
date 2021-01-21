all: ../bin/data/spectra.lut
all: ../bin/data/abney.lut

clean: clean_spec_luts

pipe/modules/spec/mkabney: pipe/modules/spec/createlut.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

pipe/modules/spec/mkspectra: pipe/modules/spec/createlut.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -DMKSPECTRA -fopenmp $< -o $@ -lm

pipe/modules/spec/macadam: pipe/modules/spec/macadam.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

clean_spec_luts:
	rm -f abney.lut spectra.lut macadam.lut

macadam.lut: pipe/modules/spec/macadam
	@echo "[spectral lut] precomputing max theoretical reflectance brightness.."
	pipe/modules/spec/macadam

../bin/data/spectra.lut: pipe/modules/spec/mkspectra macadam.lut
	@echo "[spectral lut] precomputing rgb to spectrum upsampling table.."
	pipe/modules/spec/mkspectra 512 /dev/null XYZ -b
	mv spectra.lut ../bin/data/

../bin/data/abney.lut: pipe/modules/spec/mkabney macadam.lut
	@echo "[spectral lut] precomputing saturation table to compensate the abney effect.."
	pipe/modules/spec/mkabney 2048 /dev/null XYZ
	mv abney.lut ../bin/data/
