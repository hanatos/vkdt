DB_O=\
db/db.o\
db/rc.o\
db/thumbnails.o
DB_H=\
db/db.h\
db/exif.h\
db/hash.h\
db/thumbnails.h\
db/stringpool.h
DB_CFLAGS=
DB_LDFLAGS=

ifeq ($(OS),Windows_NT)
DB_O+=db/strptime.o
endif