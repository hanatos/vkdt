.PHONY: all debug sanitize clean run

include config.mk.defaults
sinclude config.mk

all: config.mk
	$(MAKE) -C ..

debug: config.mk
	$(MAKE) -C .. debug

sanitize: config.mk
	$(MAKE) -C .. sanitize

clean:
	$(MAKE) -C .. clean
	rm -f vkdt vkdt-cli
