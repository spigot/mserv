# Process this file with automake to produce Makefile.in

INCLUDES = -I..
CTAGSFLAGS = -t -d

pkglib_LTLIBRARIES   = icecast.la

icecast_la_SOURCES = icecast.c
icecast_la_LDFLAGS = -module -avoid-version
icecast_la_LIBADD  = @MODULE_ICECAST_LIBS@ @MODULE_ICECAST_CPPFLAGS@ \
                     @MODULE_ICECAST_CFLAGS@

