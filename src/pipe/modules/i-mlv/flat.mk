MOD_C=pipe/connector.c
MOD_CFLAGS=-fopenmp
ifeq ($(CC),clang) # https://bugzilla.redhat.com/show_bug.cgi?id=2158172
MOD_CFLAGS+=-fno-openmp-implicit-rpath
endif
pipe/modules/i-mlv/libi-mlv.so: pipe/modules/i-mlv/mlv.h pipe/modules/i-mlv/raw.h pipe/modules/i-mlv/video_mlv.c pipe/modules/i-mlv/video_mlv.h
