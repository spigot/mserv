# Process this file with automake to produce Makefile.in

INCLUDES = -I../mservcli

bin_PROGRAMS = mservcmd

mservcmd_SOURCES = mservcmd.c
mservcmd_LDADD = ../mservcli/libmservcli.la @MSERVCMD_LIBS@

man1_MANS = mservcmd.1
