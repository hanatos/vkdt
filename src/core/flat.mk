CORE_O=core/log.o \
       core/threads.o
CORE_H=core/core.h \
       core/log.h \
       core/threads.h
CORE_CFLAGS=
CORE_LDFLAGS=-pthread

core/version.h: $(wildcard ../.git/FETCH_HEAD)
	@echo "#pragma once" > core/version.h
	@echo "#define VKDT_VERSION \"$(shell git describe --tags)\"" >> core/version.h

