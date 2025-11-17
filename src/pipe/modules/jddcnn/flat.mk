pipe/modules/jddcnn/*.comp.spv: pipe/modules/jddcnn/conv-impl.glsl

SHADERS=\
pipe/modules/jddcnn/con0a16.comp.spv\
pipe/modules/jddcnn/con0b16.comp.spv\
pipe/modules/jddcnn/con016.comp.spv\
pipe/modules/jddcnn/con1a16.comp.spv\
pipe/modules/jddcnn/con1b16.comp.spv\
pipe/modules/jddcnn/con116.comp.spv\
pipe/modules/jddcnn/con2a16.comp.spv\
pipe/modules/jddcnn/con2b16.comp.spv\
pipe/modules/jddcnn/con216.comp.spv\
pipe/modules/jddcnn/con3a16.comp.spv\
pipe/modules/jddcnn/con3b16.comp.spv\
pipe/modules/jddcnn/con316.comp.spv\
pipe/modules/jddcnn/con4a16.comp.spv\
pipe/modules/jddcnn/dec016.comp.spv\
pipe/modules/jddcnn/dec116.comp.spv\
pipe/modules/jddcnn/dec216.comp.spv\
pipe/modules/jddcnn/dec316.comp.spv\
pipe/modules/jddcnn/enc016.comp.spv\
pipe/modules/jddcnn/enc116.comp.spv\
pipe/modules/jddcnn/enc216.comp.spv\
pipe/modules/jddcnn/enc316.comp.spv

modules: $(SHADERS)

pipe/modules/jddcnn/%16.comp.spv: pipe/modules/jddcnn/%.comp pipe/modules/jddcnn/conv-impl.glsl
	$(GLSLC) -Ipipe/modules -Ipipe/modules/jddcnn $(GLSLC_FLAGS) -DFUINT=16 $< -o $@
