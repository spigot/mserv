# Process this file with automake to produce Makefile.in

INCLUDES = -I../hdr -DDATADIR=\"${datadir}\" -DBINDIR=\"${bindir}\"
CTAGSFLAGS = -t -d

bin_PROGRAMS = mserv

mserv_SOURCES = mserv.c misc.c cmd.c acl.c mp3info.c conf.c defconf.c \
                opt.c filter.c output-icecast.c

mserv_LDADD = @MSERV_LIBS@
mserv_CPPFLAGS = @MSERV_CPPFLAGS@
mserv_CFLAGS = @MSERV_CFLAGS@

man1_MANS = mserv.1

dist_data_DATA = english.lang
