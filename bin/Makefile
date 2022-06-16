.PHONY: all debug sanitize clean run

include config.mk.defaults
sinclude config.mk

all:
	$(MAKE) -C .. all

debug:
	$(MAKE) -C .. debug

sanitize:
	$(MAKE) -C .. sanitize

clean:
	$(MAKE) -C .. clean
	rm -f vkdt vkdt-cli vkdt-fit vkdt-mkssf vkdt-mkclut
