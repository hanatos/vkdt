MOD_LDFLAGS=-lm

pipe/modules/llap/assemble.comp.spv:pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/colour.comp.spv  :pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/curve.comp.spv   :pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/reduce.comp.spv  :pipe/modules/llap/llap.glsl pipe/modules/llap/config.h
pipe/modules/llap/libllap.$(SEXT)  :pipe/modules/llap/config.h
