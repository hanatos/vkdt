.PHONY:all clean rawspeed pthread-pool
BUILT=../built/ext

all: rawspeed pthread-pool

rawspeed: Makefile $(BUILT)/rawspeed/librawspeed.a

$(BUILT)/rawspeed/CMakeCache.txt: Makefile
	mkdir -p $(BUILT)/rawspeed
	cmake -S rawspeed/ -B $(BUILT)/rawspeed/ -DBUILD_TESTING=0

$(BUILT)/rawspeed/librawspeed.a: Makefile $(BUILT)/rawspeed/CMakeCache.txt 
	mkdir -p $(BUILT)/rawspeed
	make -C $(BUILT)/rawspeed

pthread-pool: Makefile
	mkdir -p $(BUILT)/pthread-pool
	make -C pthread-pool
	mv pthread-pool/libpthreadpool.a $(BUILT)/pthread-pool/

clean: Makefile
	make -C pthread-pool clean

