## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.5

SUBDIRS = libltdl @MY_SUBDIRS@
DIST_SUBDIRS = libltdl mserv mservcli mservutils
CTAGSFLAGS = -t -d

EXTRA_DIST = ChangeLog README NEWS AUTHORS COPYING LICENSE mservcli.pc.in

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = mservcli.pc

run:
	sudo make install
	mserv
