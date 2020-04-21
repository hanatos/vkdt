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

config.mk: config.mk.example
	@echo "you don't have a config.mk file (or config.mk.example is newer)."
	@echo "please manually 'cp config.mk.example config.mk' and edit to taste."
