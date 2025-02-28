.PHONY:tools

tools:../bin/data/spectra-em.lut ../bin/data/spectra.lut ../bin/data/abney.lut ../bin/vkdt-mkssf ../bin/vkdt-mkclut ../bin/vkdt-lutinfo ../bin/vkdt-eval-profile

ADD_CFLAGS=-Itools -Itools/shared
ADD_LDFLAGS=-lm -ldl

MKSSF_DEPS=tools/clut/src/dngproc.h\
        tools/shared/cie1931.h\
        tools/shared/matrices.h\
        tools/clut/src/cfa.h\
        tools/clut/src/cfa_data.h\
        tools/clut/src/cfa-plain.h\
        tools/clut/src/cfa-sigmoid.h\
        tools/clut/src/cfa-legendre.h\
        tools/clut/src/cfa-gauss.h

MKCLUT_DEPS=core/inpaint.h \
     core/solve.h \
     core/clip.h

../bin/vkdt-mkssf: tools/clut/src/mkssf.c ${MKSSF_DEPS} Makefile
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(ADD_CFLAGS) $< -o $@ $(LDFLAGS) $(ADD_LDFLAGS)

../bin/vkdt-mkclut: tools/clut/src/mkclut.c ${MKCLUT_DEPS} Makefile
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(ADD_CFLAGS) $< -o $@ $(LDFLAGS) $(ADD_LDFLAGS)

../bin/vkdt-lutinfo: tools/clut/src/lutinfo.c core/lut.h Makefile
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(ADD_CFLAGS) $< -o $@ $(LDFLAGS) $(ADD_LDFLAGS)

../bin/vkdt-eval-profile: tools/clut/src/eval.c ${MKSSF_DEPS} Makefile
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(ADD_CFLAGS) $< -o $@ $(LDFLAGS) $(ADD_LDFLAGS)

../bin/data/spectra.lut: ../bin/data/abney.lut tools/flat.mk
../bin/data/abney.lut: mkabney macadam.lut Makefile tools/flat.mk
	@echo "[tools] precomputing abney hue line table.."
	./mkabney
	mv abney.lut ../bin/data/
	mv spectra.lut ../bin/data/

../bin/data/spectra-em.lut: mkspectra macadam.lut Makefile
	@echo "[tools] precomputing rgb to spectrum upsampling table.."
	./mkspectra
	mv spectra-em.lut ../bin/data/

macadam.lut: macadam
	./macadam

macadam: tools/spec/macadam.c core/threads.c Makefile
	@echo "[tools] precomputing max theoretical reflectance brightness.."
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(EXE_CFLAGS) $(ADD_CFLAGS) $< core/threads.c -o $@ $(LDFLAGS) $(ADD_LDFLAGS) -pthread

mkspectra: tools/spec/mkspectra.c core/threads.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(EXE_CFLAGS) $(ADD_CFLAGS) $< core/threads.c -o $@ $(LDFLAGS) $(ADD_LDFLAGS) -pthread

mkabney: tools/spec/mkabney.c core/threads.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(EXE_CFLAGS) $(ADD_CFLAGS) $< core/threads.c -o $@ $(LDFLAGS) $(ADD_LDFLAGS) -pthread
