.PHONY: all debug sanitize sanitize-thread cli reload-shaders clean

all:
	$(MAKE) -C .. all

debug:
	$(MAKE) -C .. debug

sanitize:
	$(MAKE) -C .. sanitize

sanitize-thread:
	$(MAKE) -C .. sanitize-thread

cli:
	$(MAKE) -C .. cli

lib:
	$(MAKE) -C .. lib

reload-shaders:
	$(MAKE) -C .. reload-shaders

clean:
	$(MAKE) -C .. clean
	rm -f vkdt vkdt-cli vkdt-fit vkdt-mkssf vkdt-mkclut vkdt-eval-profile vkdt-lutinfo libvkdt.*
