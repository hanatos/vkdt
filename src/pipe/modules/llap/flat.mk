MOD_LDFLAGS=-lm
MOD_C=pipe/connector.c

pipe/modules/llap/assemble.comp.spv:pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/colour.comp.spv  :pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/curve.comp.spv   :pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/reduce.comp.spv  :pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/libllap.so       :pipe/modules/llap/config.h
