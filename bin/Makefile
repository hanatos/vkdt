.PHONY: all debug sanitize clean run

include config.mk

all: config.mk
	$(MAKE) -C ..

debug: config.mk
	$(MAKE) -C .. debug

sanitize: config.mk
	$(MAKE) -C .. sanitize

clean:
	$(MAKE) -C .. clean
	rm -f vkdt vkdt-cli

config.mk: config.mk.example
	$(error config.mk does not exist or config.mk.example is newer. please update your config.mk!)
