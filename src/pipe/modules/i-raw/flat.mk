# use rawspeed
ifeq ($(VKDT_USE_RAWINPUT),1)
RS_COMMIT=2baa3a2
RAWSPEED_I=pipe/modules/i-raw/rawspeed-$(RS_COMMIT)
RAWSPEED_L=$(RAWSPEED_I)/build
MOD_CFLAGS=-std=c++20 -Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/ $(VKDT_PUGIXML_CFLAGS) $(VKDT_JPEG_CFLAGS)
MOD_LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lz $(VKDT_PUGIXML_LDFLAGS) $(VKDT_JPEG_LDFLAGS)

pipe/modules/i-raw/libi-raw.so: $(RAWSPEED_L)/librawspeed.a

ifeq ($(CXX),clang++)
MOD_LDFLAGS+=-fopenmp=libomp
endif
ifeq ($(CXX),g++)
MOD_LDFLAGS+=-lgomp
endif


$(RAWSPEED_L)/Makefile: $(RAWSPEED_I)/CMakeLists.txt
	mkdir -p $(RAWSPEED_L)
ifeq ($(RAWSPEED_PACKAGE_BUILD),0)
	cd $(RAWSPEED_L); $(CMAKE) .. -G "Unix Makefiles" -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_WERROR=OFF -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBUILD_BENCHMARKING=off -DBUILD_FUZZERS=off -DBUILD_TOOLS=off
else
	cd $(RAWSPEED_L); $(CMAKE) .. -G "Unix Makefiles" -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_WERROR=OFF -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBUILD_BENCHMARKING=off -DBUILD_FUZZERS=off -DBUILD_TOOLS=off -DBINARY_PACKAGE_BUILD=on
endif

$(RAWSPEED_L)/librawspeed.a: $(RAWSPEED_L)/Makefile
	cp $(RAWSPEED_I)/data/cameras.xml ../bin/data
	$(MAKE) -C $(RAWSPEED_L)
	strip -S $(RAWSPEED_L)/librawspeed.a

$(RAWSPEED_I)/CMakeLists.txt:
	rm -rf pipe/modules/i-raw/rawspeed-*
	git clone https://github.com/darktable-org/rawspeed.git --branch develop --single-branch $(RAWSPEED_I)
	cd $(RAWSPEED_I); git reset --hard $(RS_COMMIT)

ifeq ($(VKDT_USE_EXIV2),1)
MOD_CFLAGS+=$(VKDT_EXIV2_CFLAGS) -DVKDT_USE_EXIV2=1
MOD_LDFLAGS+=$(VKDT_EXIV2_LDFLAGS)
pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/exif.h
endif
# TODO: cache a hash to the checkout revision and test against what we want
endif # end rawspeed

# use rawloader
ifeq ($(VKDT_USE_RAWINPUT),2)
# this takes ages to determine for incremental builds/when changing
# branches. thus, hardcode to whatever the 'eval' line here would output:
# RUST_LDFLAGS=$(eval RUST_LDFLAGS := $$(shell cd pipe/modules/i-raw/rawloader-c; cargo rustc -- --print native-static-libs 2>&1 | grep native-static-libs | cut -d: -f3))$(RUST_LDFLAGS)
ifeq ($(OS),Windows_NT)
RUST_LDFLAGS=-lkernel32 -lntdll -luserenv -lws2_32 -ldbghelp -lsynchronization
else
ifeq ($(shell uname),Darwin)
RUST_LDFLAGS=-liconv -framework CoreFoundation -lSystem -lc -lm
else
RUST_LDFLAGS=-ldl -lgcc_s -lutil -lrt -lpthread -lm -ldl -lc
endif
endif
MOD_LDFLAGS=pipe/modules/i-raw/rawloader-c/target/release/librawloader.a -lm
MOD_LDFLAGS+=$(RUST_LDFLAGS)
MOD_CFLAGS=-Ipipe/modules/i-raw/rawloader-c
pipe/modules/i-raw/libi-raw.so: pipe/modules/i-raw/rawloader-c/target/release/librawloader.a

pipe/modules/i-raw/rawloader-c/target/release/librawloader.a: pipe/modules/i-raw/rawloader-c/lib.rs pipe/modules/i-raw/rawloader-c/Cargo.toml 
	echo $(RUST_LDFLAGS)
	cd pipe/modules/i-raw/rawloader-c; cargo build --release
	touch pipe/modules/i-raw/rawloader-c/target/release/librawloader.a
endif

pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/dng_opcode_decode.c
