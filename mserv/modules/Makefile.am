# Process this file with automake to produce Makefile.in

INCLUDES = -I..
CTAGSFLAGS = -t -d

pkglib_LTLIBRARIES =

if WANT_MODULE_ICECAST
  pkglib_LTLIBRARIES += icecast.la
endif

if WANT_MODULE_OSSAUDIO
  pkglib_LTLIBRARIES += ossaudio.la
endif

icecast_la_SOURCES = icecast.c
icecast_la_LDFLAGS = -module -avoid-version
icecast_la_LIBADD  = @MODULE_ICECAST_LIBS@ @MODULE_ICECAST_CPPFLAGS@ \
                     @MODULE_ICECAST_CFLAGS@

ossaudio_la_SOURCES = ossaudio.c
ossaudio_la_LDFLAGS = -module -avoid-version
ossaudio_la_LIBADD  = @MODULE_OSSAUDIO_LIBS@ @MODULE_OSSAUDIO_CPPFLAGS@ \
                      @MODULE_OSSAUDIO_CFLAGS@

