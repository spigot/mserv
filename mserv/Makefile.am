# Process this file with automake to produce Makefile.in

INCLUDES = -I../hdr -DDATADIR=\"${datadir}\"

bin_PROGRAMS = mserv

mserv_SOURCES = mserv.c misc.c cmd.c acl.c mp3info.c conf.c defconf.c \
                opt.c filter.c

mserv_LDADD = @MY_LIBS@

man1_MANS = mserv.1

dist_data_DATA = english.lang
