.PHONY:all ext src clean distclean

all: ext src

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
