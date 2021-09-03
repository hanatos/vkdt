.PHONY:all clean rawspeed
BUILT=../built/ext

all: rawspeed

rawspeed: Makefile $(BUILT)/rawspeed/librawspeed.a

$(BUILT)/rawspeed/Makefile: Makefile
ifeq ($(VKDT_USE_RAWSPEED),0)
  touch $(BUILT)/rawspeed/Makefile
else
ifeq ($(RAWSPEED_PACKAGE_BUILD),0)
	mkdir -p $(BUILT)/rawspeed
	cd $(BUILT)/rawspeed; cmake ../../../ext/rawspeed -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off
else
	mkdir -p $(BUILT)/rawspeed
	cd $(BUILT)/rawspeed; cmake ../../../ext/rawspeed -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBINARY_PACKAGE_BUILD=on
endif
endif

# for some reason this is challenging for older? cmake
# cmake -S rawspeed/ -B $(BUILT)/rawspeed/ -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release

$(BUILT)/rawspeed/librawspeed.a: Makefile $(BUILT)/rawspeed/Makefile
	mkdir -p $(BUILT)/rawspeed
ifeq ($(VKDT_USE_RAWSPEED),0)
else
	$(MAKE) -C $(BUILT)/rawspeed
	strip -S $(BUILT)/rawspeed/librawspeed.a
endif

clean: Makefile

