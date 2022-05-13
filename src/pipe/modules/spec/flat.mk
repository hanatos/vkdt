all: ../bin/data/spectra.lut

# i'm annoyed that these keep regenerating:
# clean: clean_spec_luts

pipe/modules/spec/mkspectra: pipe/modules/spec/createlut.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -DMKSPECTRA -fopenmp $< -o $@ -lm

pipe/modules/spec/macadam: pipe/modules/spec/macadam.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

clean_spec_luts:
	rm -f spectra.lut macadam.lut

macadam.lut: pipe/modules/spec/macadam
	@echo "[spectral lut] precomputing max theoretical reflectance brightness.."
	pipe/modules/spec/macadam

../bin/data/spectra.lut: pipe/modules/spec/mkspectra macadam.lut
	@echo "[spectral lut] precomputing rgb to spectrum upsampling table.."
	pipe/modules/spec/mkspectra 512 /dev/null XYZ -b
	mv spectra.lut ../bin/data/
