dnl Mserv configure.in file - process with autoconf

  AC_INIT(mserv/mserv.c)
  AC_PREREQ(2.52)
  AC_CANONICAL_SYSTEM
  AM_INIT_AUTOMAKE(mserv, 0.41)
  AM_CONFIG_HEADER(config.h)
  AC_CONFIG_SUBDIRS(libltdl)

dnl Checks for header files

  AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics

  AC_C_CONST
  AC_C_INLINE

dnl Checks for functions

  AC_FUNC_SNPRINTF
  AC_CHECK_FUNCS(strsep)

dnl libtool / libltdl

  AC_LIBTOOL_DLOPEN
  AC_LIBLTDL_CONVENIENCE
  AC_PROG_LIBTOOL 
  AC_SUBST(INCLTDL)
  AC_SUBST(LIBLTDL)

dnl Checks for programs.

  AC_PROG_CC
  AC_PROG_RANLIB
  AC_PROG_MAKE_SET
  AC_ISC_POSIX
  AC_PROG_INSTALL
  AC_TYPE_SIGNAL

  AC_CHECK_PROG(make, gmake, gmake, make)

dnl Options

  AC_ARG_ENABLE(module-ossaudio,
    AC_HELP_STRING([--enable-module-ossaudio], [build ossaudio module]),,
    enable_module_ossaudio="yes")
  AC_ARG_ENABLE(with-ossaudio-mixer,
    AC_HELP_STRING([--with-ossaudio-mixer=PATH], [path to ossaudio mixer]),,
    with_mixer="/dev/mixer")

  AC_ARG_ENABLE(module-icecast,
    AC_HELP_STRING([--enable-engine-icecast], [build icecast module]),,
    enable_module_icecast="yes")

  AC_ARG_WITH(warnings,
    AC_HELP_STRING([--with-warnings], [turn on gcc warnings]))
  AC_ARG_WITH(debug,
    AC_HELP_STRING([--with-debug], [turn debugging options on]))

  AC_ARG_ENABLE(server,
    AC_HELP_STRING([--disable-server], [do not compile server]),,
    enable_server="yes")
  AC_ARG_ENABLE(client,
    AC_HELP_STRING([--disable-client], [do not compile client libs/utils]),,
    enable_client="yes")

  AC_ARG_WITH(libsamplerate,
    AC_HELP_STRING([--with-libsamplerate=DIR], [use libsamplerate]),,
    with_LIBSAMPLERATE=not-specified)

dnl Component parts

  AC_MSG_RESULT()
  AC_MSG_RESULT([Components:])

  AC_SUBST(MY_SUBDIRS, "")
  if test "x$enable_server" = "xyes"; then
    MY_SUBDIRS="$MY_SUBDIRS mserv"
    AC_MSG_RESULT([  mserv (server)])
  else
    AC_MSG_RESULT([  not building mserv (server)])
  fi
  if test "x$enable_client" = "xyes"; then
    MY_SUBDIRS="$MY_SUBDIRS mservcli mservutils"
    AC_MSG_RESULT([  mservcli (client library)])
    AC_MSG_RESULT([  mservutils (command line utils)])
  else
    AC_MSG_RESULT([  not building mservcli (client library)])
    AC_MSG_RESULT([  not building mservutils (command line utils)])
  fi

dnl mservcli

  AC_MSG_RESULT()
  AC_MSG_RESULT([Checking for: mservcli])
  AC_SUBST(MSERVCLI_LIBS, "")
  AC_CHECK_LIB(socket, main, MSERVCLI_LIBS="$MSERVCLI_LIBS -lsocket")
  AC_CHECK_LIB(malloc, main, MSERVCLI_LIBS="$MSERVCLI_LIBS -lmalloc")

dnl mservcmd

  AC_MSG_RESULT()
  AC_MSG_RESULT([Checking for: mservcmd])
  AC_SUBST(MSERVCMD_LIBS, "")
  AC_CHECK_LIB(socket, main, MSERVCMD_LIBS="$MSERVCMD_LIBS -lsocket")
  AC_CHECK_LIB(malloc, main, MSERVCMD_LIBS="$MSERVCMD_LIBS -lmalloc")

dnl mserv

  AC_MSG_RESULT()
  AC_MSG_RESULT([Checking for: mserv])
  AC_SUBST(MSERV_LIBS, "")
  AC_SUBST(MSERV_CFLAGS, "")
  AC_SUBST(MSERV_CPPFLAGS, "")
  AC_CHECK_LIB(m, pow, MSERV_LIBS="$MSERV_LIBS -lm")
  AC_CHECK_LIB(socket, main, MSERV_LIBS="$MSERV_LIBS -lsocket")
  AC_CHECK_LIB(malloc, main, MSERV_LIBS="$MSERV_LIBS -lmalloc")
  AC_CHECK_LIB(crypt, crypt, MSERV_LIBS="$MSERV_LIBS -lcrypt")

  if test "x$with_LIBSAMPLERATE" = "xnot-specified"; then
    PKG_CHECK_MODULES(LIBSAMPLERATE, samplerate,,
      AC_WARN([$LIBSAMPLERATE_PKG_ERRORS]); with_LIBSAMPLERATE=no)
    if test "x$with_LIBSAMPLERATE" = "xnot-specified"; then
      MSERV_LIBS="$MSERV_LIBS $LIBSAMPLERATE_LIBS"
      MSERV_CFLAGS="$MSERV_CFLAGS $LIBSAMPLERATE_CFLAGS"
    fi
  else
    if test "x$with_LIBSAMPLERATE" != "xno"; then
      MSERV_CFLAGS="$MSERV_CFLAGS -L$with_LIBSAMPLERATE/lib"
      MSERV_CFLAGS="$MSERV_CFLAGS -I$with_LIBSAMPLERATE/include"
      MSERV_LIBS="$MSERV_LIBS -lsamplerate"
    fi
  fi
  if test "x$with_LIBSAMPLERATE" != "xno"; then
    AC_DEFINE_UNQUOTED(HAVE_LIBSAMPLERATE, 1,
                       [libsamplerate will be used if defined])
  else
    AC_MSG_RESULT([libsamplerate not enabled])
  fi

dnl mserv modules

  AC_MSG_RESULT()
  AC_MSG_RESULT([Checking for: mserv modules])
  AC_SUBST(MODULE_ICECAST_LIBS, "")
  AC_SUBST(MODULE_ICECAST_CPPFLAGS, "")
  AC_SUBST(MODULE_ICECAST_CFLAGS, "")
  if test "x$enable_module_icecast" = "xyes"; then
    L=""
    XIPH_PATH_SHOUT(, AC_MSG_ERROR([must have libshout installed!]))
    AC_CHECK_LIB(pthread, pthread_create, L="$L -lpthread")
    if test "$SHOUT_THREADSAFE" != "yes"; then
      AC_MSG_ERROR([This libshout isn't threadsafe])
    fi
    XIPH_PATH_VORBIS(, AC_MSG_ERROR([must have Ogg Vorbis v1.0 installed!]))
    MODULE_ICECAST_LIBS="$L $SHOUT_LIBS $VORBISENC_LIBS $VORBIS_LIBS"
    MODULE_ICECAST_CPPFLAGS="$SHOUT_CPPFLAGS $VORBIS_CPPFLAGS"
    MODULE_ICECAST_CFLAGS="$SHOUT_CFLAGS $VORBIS_CFLAGS"
  fi

  AC_SUBST(MODULE_OSSAUDIO_LIBS, "")
  AC_SUBST(MODULE_OSSAUDIO_CPPFLAGS, "")
  AC_SUBST(MODULE_OSSAUDIO_CFLAGS, "")
  if test "x$enable_module_ossaudio" = "xyes"; then
    L=""
    AC_CHECK_LIB(ossaudio, _oss_ioctl, L="$L -lossaudio")
    MODULE_OSSAUDIO_LIBS="$L"
    FOUND_SOUNDCARD=no
    AC_CHECK_HEADERS(sys/soundcard.h, FOUND_SOUNDCARD=yes)
    AC_CHECK_HEADERS(soundcard.h, FOUND_SOUNDCARD=yes)
    if test "x$FOUND_SOUNDCARD" = "xno"; then
      AC_MSG_ERROR([cannot find soundcard.h])
    fi
    if test "x$with_ossaudio_mixer" != "xno"; then
      AC_DEFINE_UNQUOTED(OSSAUDIO_MIXER, $with_ossaudio_mixer,
                         [path to ossaudio mixer])
    fi
  fi

  AC_MSG_RESULT()
  AC_MSG_RESULT([Modules:])

  AM_CONDITIONAL(WANT_MODULE_ICECAST, test "x$enable_module_icecast" = "xyes")
  if test "x$enable_module_icecast" = "xyes"; then
    AC_MSG_RESULT([  icecast enabled])
  else
    AC_MSG_RESULT([  icecast disabled])
  fi

  AM_CONDITIONAL(WANT_MODULE_OSSAUDIO, test "x$enable_module_ossaudio" = "xyes")
  if test "x$enable_module_ossaudio" = "xyes"; then
    AC_MSG_RESULT([  ossaudio enabled])
  else
    AC_MSG_RESULT([  ossaudio disabled])
  fi

  AC_MSG_RESULT()

dnl C flags etc.

  if test "x$with_warnings" = "xyes"; then
    AC_MSG_RESULT(Turning on warning flags)
    CFLAGS="$CFLAGS -W -Wall -Wstrict-prototypes -Wformat -Wunused -Wsign-compare -Winline -Wdisabled-optimization -Werror"
  fi

  if test "x$with_debug" = "xyes"; then
    AC_MSG_RESULT(Turning on debug flags)
    CFLAGS="$CFLAGS -g"
  fi

dnl Final stuff

  AC_OUTPUT([Makefile mserv/Makefile mservcli/Makefile
             mservutils/Makefile mserv/modules/Makefile
             mservcli.pc])

dnl Moooo.
