dnl Mserv configure.in file - process with autoconf

  AC_INIT(mserv/mserv.c)
  AC_PREREQ(2.52)
  AC_CANONICAL_SYSTEM
  AM_INIT_AUTOMAKE(mserv, 0.39)
  AM_CONFIG_HEADER(config.h)

dnl Engine selection

  ENGINE="local"
  AC_ARG_WITH(engine-local, AC_HELP_STRING([--with-engine-local],
              [Use local engine (default)]),
              ENGINE="local")
  AC_ARG_WITH(engine-icecast, AC_HELP_STRING([--with-engine-icecast],
              [Use icecast engine]),
              ENGINE="icecast")
  AC_DEFINE_UNQUOTED(ENGINE, $ENGINE, [Engine type])

dnl Checks for programs.

  AC_PROG_CC
  AC_PROG_RANLIB
  AC_PROG_MAKE_SET
  AC_ISC_POSIX
  AC_PROG_INSTALL
  AC_TYPE_SIGNAL
  AC_PROG_LIBTOOL 

  AC_CHECK_PROG(make, gmake, gmake, make)

  AC_ARG_WITH(warnings, AC_HELP_STRING([--with-warnings],
                                       [Turn on gcc warnings]),
              WARN=$withval, WARN=no)
  AC_ARG_WITH(debug, AC_HELP_STRING([--with-debug],
                                    [Turn debugging options on]),
              DEBUG=$withval, DEBUG=no)

  if [[ "x$WARN" != "xno" ]]; then
    CFLAGS="$CFLAGS -W -Wall -Wstrict-prototypes -Wformat -Wunused -Wsign-compar
e -Wunreachable-code -Winline -Wdisabled-optimization"
  fi

  if [[ "x$DEBUG" != "xno" ]]; then
    AC_MSG_RESULT(Turning on debug flags)
    CFLAGS="$CFLAGS -g"
  fi

  AC_ARG_ENABLE(server,
                AC_HELP_STRING([--disable-server], [Do not compile server]),
                COMPILE_SERVER=$withval, COMPILE_SERVER=yes)

  AC_ARG_ENABLE(client, AC_HELP_STRING([--disable-client],
                [Do not compile client libs/utils]),
                COMPILE_CLIENT=$withval, COMPILE_CLIENT=yes)

  MY_SUBDIRS=""
  if [[ "x$COMPILE_SERVER" = "xyes" ]]; then
    MY_SUBDIRS="$MY_SUBDIRS mserv"
  fi
  if [[ "x$COMPILE_CLIENT" = "xyes" ]]; then
    MY_SUBDIRS="$MY_SUBDIRS mservcli mservutils"
  fi

dnl Checks for libraries - mserv

  AC_MSG_RESULT(Checking libraries for mserv)
  MSERV_LIBS=""
  AC_CHECK_LIB(m, pow, MSERV_LIBS="$MSERV_LIBS -lm")
  AC_CHECK_LIB(socket, main, MSERV_LIBS="$MSERV_LIBS -lsocket")
  AC_CHECK_LIB(malloc, main, MSERV_LIBS="$MSERV_LIBS -lmalloc")
  AC_CHECK_LIB(crypt, crypt, MSERV_LIBS="$MSERV_LIBS -lcrypt")
  AC_CHECK_LIB(ossaudio, _oss_ioctl, MSERV_LIBS="$MSERV_LIBS -lossaudio")

  if [[ "x$ENGINE" = "xicecast" ]]; then
    XIPH_PATH_SHOUT(, AC_MSG_ERROR([must have libshout installed!]))
    AC_CHECK_LIB(pthread, pthread_create, MSERV_LIBS="$MSERV_LIBS -lpthread")
    if test "$SHOUT_THREADSAFE" != "yes"; then
      AC_MSG_ERROR([This libshout isn't threadsafe])
    fi
    XIPH_PATH_VORBIS(, AC_MSG_ERROR([must have Ogg Vorbis v1.0 installed!]))
    MSERV_LIBS="$MSERV_LIBS $SHOUT_LIBS $VORBISENC_LIBS $VORBIS_LIBS"
    MSERV_CPPFLAGS="$MSERV_CPPFLAGS $SHOUT_CPPFLAGS $VORBIS_CPPFLAGS"
    MSERV_CFLAGS="$MSERV_CFLAGS $SHOUT_CFLAGS $VORBIS_CFLAGS"
  fi

dnl Checks for libraries - mservcli

  AC_MSG_RESULT(Checking libraries for libmservcli)
  MSERVCLI_LIBS=""
  AC_CHECK_LIB(socket, main, MSERVCLI_LIBS="$MSERVCLI_LIBS -lsocket")
  AC_CHECK_LIB(malloc, main, MSERVCLI_LIBS="$MSERVCLI_LIBS -lmalloc")

dnl Checks for libraries - mservcmd

  AC_MSG_RESULT(Checking libraries for mservcmd)
  MSERVCMD_LIBS=""
  AC_CHECK_LIB(socket, main, MSERVCMD_LIBS="$MSERVCMD_LIBS -lsocket")
  AC_CHECK_LIB(malloc, main, MSERVCMD_LIBS="$MSERVCMD_LIBS -lmalloc")

dnl Checks for header files

  AC_HEADER_STDC

  FOUND_SOUNDCARD=no
  AC_CHECK_HEADERS(sys/soundcard.h, FOUND_SOUNDCARD=yes)
  AC_CHECK_HEADERS(soundcard.h, FOUND_SOUNDCARD=yes)

dnl Checks for typedefs, structures, and compiler characteristics

  AC_C_CONST
  AC_C_INLINE

dnl Checks for functions

  AC_CHECK_FUNCS(strsep)

dnl Makefile substitutions

  AC_SUBST(DEPFLAGS)
  AC_SUBST(MSERV_LIBS)
  AC_SUBST(MSERV_CFLAGS)
  AC_SUBST(MSERV_CPPFLAGS)
  AC_SUBST(MSERVCLI_LIBS)
  AC_SUBST(MSERVCMD_LIBS)
  AC_SUBST(MY_SUBDIRS)

dnl Final stuff

  AC_OUTPUT([Makefile mserv/Makefile mservcli/Makefile
             mservutils/Makefile])

  AC_MSG_RESULT()
  if [[ "x$COMPILE_SERVER" = "xyes" ]]; then
    AC_MSG_RESULT([  Server components will be compiled])
  else
    AC_MSG_RESULT([  Server components will not be compiled])
  fi

  if [[ "x$COMPILE_CLIENT" = "xyes" ]]; then
    AC_MSG_RESULT([  Client components will be compiled])
  else
    AC_MSG_RESULT([  Client components will not be compiled])
  fi

  if [[ "x$ENGINE" = "xlocal" ]]; then
    AC_MSG_ERROR([  Local output engine not support in this release])
    AC_MSG_RESULT([  Local output engine selected]);
    if [[ "x$FOUND_SOUNDCARD" = "xno" ]]; then
      echo "WARNING: no soundcard.h! VOLUME/BASS/TREBLE controls disabled!";
    fi
  elif [[ "x$ENGINE" = "xicecast" ]]; then
    AC_MSG_RESULT([  Icecast output engine selected])
  fi
  AC_MSG_RESULT()

dnl Moooo.
