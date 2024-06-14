MOD_CFLAGS=-fopenmp
pipe/modules/i-mlv/libi-mlv.so: pipe/modules/i-mlv/mlv.h pipe/modules/i-mlv/raw.h pipe/modules/i-mlv/video_mlv.c pipe/modules/i-mlv/video_mlv.h pipe/modules/i-mlv/liblj92/lj92.c
ifeq ($(CXX),clang++)
MOD_LDFLAGS+=-fopenmp=libomp
endif
ifeq ($(CXX),g++)
MOD_LDFLAGS+=-lgomp
endif
