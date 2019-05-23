.PHONY:all ext src clean distclean bin

all: ext src bin

ext: Makefile
	mkdir -p built/
	make -C ext/

src: ext Makefile
	mkdir -p built/
	make -C src/

clean:
	make -C ext/ clean
	make -C src/ clean

distclean:
	rm -rf built/
	rm -rf bin/

bin: src Makefile
	mkdir -p bin/data
	cp src/cli/vkdt-cli bin/
	ln -sf ../src/pipe/modules bin/
	cp ext/rawspeed/data/cameras.xml bin/data
