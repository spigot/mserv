# Process this file with automake to produce Makefile.in

INCLUDES = -I../hdr -DDATADIR=\"${datadir}\" -DBINDIR=\"${bindir}\" \
           -DPKGLIBDIR=\"${pkglibdir}\" @INCLTDL@
CTAGSFLAGS = -t -d

SUBDIRS = modules
DIST_SUBDIRS = modules

bin_PROGRAMS = mserv

mserv_SOURCES = mserv.c misc.c acl.c mp3info.c conf.c defconf.c \
                opt.c filter.c channel.c module.c params.c \
                cmd.c cmd_x.c cmd_set.c cmd_channel.c

mserv_LDADD = @MSERV_LIBS@ @LIBLTDL@
mserv_LDFLAGS = -export-dynamic
mserv_CPPFLAGS = @MSERV_CPPFLAGS@
mserv_CFLAGS = @MSERV_CFLAGS@

man1_MANS = mserv.1

dist_data_DATA = english.lang
