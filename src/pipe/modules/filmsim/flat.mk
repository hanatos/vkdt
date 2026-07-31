pipe/modules/filmsim/libfilmsim.so: pipe/modules/filmsim/wb.h pipe/modules/filmsim/pos_wb.h
FILMSIM_GLSL_DEPS=pipe/modules/filmsim/constants.h pipe/modules/filmsim/head.glsl pipe/modules/filmsim/data.glsl pipe/modules/filmsim/setup.glsl pipe/modules/filmsim/filmsim.glsl pipe/modules/filmsim/state.glsl pipe/modules/filmsim/state_ops.glsl pipe/modules/filmsim/sampling.glsl pipe/modules/filmsim/develop.glsl pipe/modules/filmsim/exposure.glsl pipe/modules/filmsim/print.glsl pipe/modules/filmsim/scan.glsl pipe/modules/filmsim/halation.glsl pipe/modules/shared/upsample.glsl
pipe/modules/filmsim/negprint.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/expose.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/develop.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/halin.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/halout.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/scatter.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/dirlut.comp.spv: $(FILMSIM_GLSL_DEPS)
pipe/modules/filmsim/setup.comp.spv: $(FILMSIM_GLSL_DEPS)
