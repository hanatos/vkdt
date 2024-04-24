MCRAW_BRANCH=main
# this is upstream, use as soon as this works:
# MCRAW_COMMIT=d0de7fa
# MCRAW_REPO=https://github.com/mirsadm/motioncam-decoder.git 
# repo with non-decoding api interface for gpu processing:
MCRAW_COMMIT=1b037ea
MCRAW_REPO=https://github.com/hanatos/motioncam-decoder.git
MCRAW_I=pipe/modules/i-mcraw/mcraw-$(MCRAW_COMMIT)
MCRAW_L=$(MCRAW_I)/build
MOD_CFLAGS=-std=c++17 -Wall -I$(MCRAW_I)/lib/include -I$(MCRAW_I)/thirdparty
MOD_LDFLAGS=-L$(MCRAW_L) -lmotioncam_decoder

pipe/modules/i-mcraw/libi-mcraw.so: $(MCRAW_L)/libmotioncam_decoder.a

$(MCRAW_L)/Makefile: $(MCRAW_I)/CMakeLists.txt
	mkdir -p $(MCRAW_L)
	cd $(MCRAW_L); $(CMAKE) .. -G "Unix Makefiles"

$(MCRAW_L)/libmotioncam_decoder.a: $(MCRAW_L)/Makefile
	$(MAKE) -C $(MCRAW_L)
	strip -S $(MCRAW_L)/libmotioncam_decoder.a

$(MCRAW_I)/CMakeLists.txt:
	rm -rf pipe/modules/i-mcraw/mcraw-*
	git clone $(MCRAW_REPO) --branch $(MCRAW_BRANCH) --single-branch $(MCRAW_I)
	cd $(MCRAW_I); git reset --hard $(MCRAW_COMMIT)
