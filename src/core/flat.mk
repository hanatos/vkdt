CORE_O=core/log.o \
       core/threads.o
CORE_H=core/colour.h \
       core/core.h \
       core/log.h \
       core/mat3.h \
       core/threads.h
CORE_CFLAGS=
CORE_LDFLAGS=-pthread -ldl

ifeq ($(OS),Windows_NT)
CORE_O+=core/utf8_manifest.o
core/utf8_manifest.o: core/utf8.rc
	windres -O coff $< $@
endif
