AC_INIT([libcloudstorage], [1.2])
AM_INIT_AUTOMAKE([foreign subdir-objects tar-pax])
AC_CONFIG_MACRO_DIRS([m4])
AC_USE_SYSTEM_EXTENSIONS

AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

AC_LANG(C++)

LT_INIT([win32-dll shared disable-static])

AC_PROG_CXX

AX_CXX_COMPILE_STDCXX_11

PKG_CHECK_MODULES([libjsoncpp], [jsoncpp])
PKG_CHECK_MODULES([libtinyxml2], [tinyxml2])

HAVE_MEGA=0
AC_ARG_WITH([mega], AS_HELP_STRING([--with-mega]))
AS_IF([test "x$with_mega" != "xno"], [
  PKG_CHECK_MODULES([libmega], [libmega], [
    HAVE_MEGA=1
    AC_DEFINE(WITH_MEGA)
  ], [
    HAVE_MEGA=0
    AS_IF([test "x$with_mega" = "xyes"], [AC_MSG_ERROR([libmega not found])])
  ])
])
AM_CONDITIONAL([WITH_MEGA], [test "$HAVE_MEGA" -eq 1])

HAVE_CRYPTOPP=0
AC_ARG_WITH([cryptopp], AS_HELP_STRING([--with-cryptopp]))
AS_IF([test "x$with_cryptopp" != "xno"], [
  PKG_CHECK_MODULES([libcryptopp], [libcrypto++], [
    HAVE_CRYPTOPP=1
    AC_DEFINE(WITH_CRYPTOPP)
  ], [
    HAVE_CRYPTOPP=0
    AS_IF([test "x$with_mega" = "xyes"], [AC_MSG_ERROR([libcrypto++ not found])])
  ])
])
AM_CONDITIONAL([WITH_CRYPTOPP], [test "$HAVE_CRYPTOPP" -eq 1])

HAVE_CURL=0
AC_ARG_WITH([curl], AS_HELP_STRING([--with-curl]))
AS_IF([test "x$with_curl" != "xno"], [
  PKG_CHECK_MODULES([libcurl], [libcurl], [
    HAVE_CURL=1
    AC_DEFINE(WITH_CURL)
  ], [
    HAVE_CURL=0
    AS_IF([test "x$with_curl" = "xyes"], [AC_MSG_ERROR([libcurl not found])])
  ])
])
AM_CONDITIONAL([WITH_CURL], [test "$HAVE_CURL" -eq 1])

HAVE_MICROHTTPD=0
AC_ARG_WITH([microhttpd], AS_HELP_STRING([--with-microhttpd]))
AS_IF([test "x$with_microhttpd" != "xno"], [
  PKG_CHECK_MODULES([libmicrohttpd], [libmicrohttpd], [
    HAVE_MICROHTTPD=1
    AC_DEFINE(WITH_MICROHTTPD)
  ], [
    HAVE_MICROHTTPD=0
    AS_IF([test "x$with_microhttpd" = "xyes"], [AC_MSG_ERROR([libmicrohttpd not found])])
  ])
])
AM_CONDITIONAL([WITH_MICROHTTPD], [test "$HAVE_MICROHTTPD" -eq 1])

FILESYSTEM_LIBS=""
AC_ARG_WITH([filesystem], AS_HELP_STRING([--with-filesystem]))
AS_IF([test "x$with_filesystem" != "xno"], [
  AC_CHECK_HEADERS(boost/filesystem.hpp, [
    FILESYSTEM_LIBS="-lboost_system -lboost_filesystem"
  ], [
    AS_IF([test "x$with_filesystem" = "xyes"], [AC_MSG_ERROR([boost-filesystem not found])])
  ])
])
AC_SUBST(FILESYSTEM_LIBS)

HAVE_FUSE=0
HAVE_LEGACY_FUSE=0
AC_ARG_WITH([fuse], AS_HELP_STRING([--with-fuse]))
AS_IF([test "x$with_fuse" != "xno"], [
  PKG_CHECK_MODULES([fuse], [fuse3], [
    HAVE_FUSE=1
    AC_DEFINE(WITH_FUSE)
  ], [
    PKG_CHECK_MODULES([fuse2], [fuse], [
      HAVE_LEGACY_FUSE=1
      AC_DEFINE(WITH_LEGACY_FUSE)
    ], [
      AS_IF([test "x$with_fuse" = "xyes"], [AC_MSG_ERROR([fuse3 not found])])
    ])
  ])
])
AM_CONDITIONAL([WITH_FUSE], [test "$HAVE_FUSE" -eq 1])
AM_CONDITIONAL([WITH_LEGACY_FUSE], [test "$HAVE_LEGACY_FUSE" -eq 1])

AC_ARG_ENABLE([token-file], AC_HELP_STRING([--enable-token-file], [path to the json file with tokens]))
AS_IF([test "x$enable_token_file" != "x"], [
  AC_DEFINE_UNQUOTED(TOKEN_FILE, "${enable_token_file}")
])

case "${host_os}" in
  *mingw32*)
    SYS=mingw32
    ;;
  *)
    SYS=linux
    ;;
esac

AC_CONFIG_FILES([
  Makefile
  src/libcloudstorage.pc
  src/Makefile
  test/Makefile
  bin/Makefile
  bin/fuse/Makefile
  bin/cloudbrowser/Makefile
  examples/Makefile
  examples/commandlinebrowser/Makefile
])

SOCKET_LIBS=""
AS_IF([test "${SYS}" = "mingw32"], [
  SOCKET_LIBS="-lws2_32"
])
AC_SUBST(SOCKET_LIBS)

AC_ARG_WITH([examples], AS_HELP_STRING([--with-examples]))
AM_CONDITIONAL([EXAMPLES], [test "x$with_examples" = "xyes"])

HAVE_QT=0
HAVE_QTWEBVIEW=0
HAVE_THUMBNAILER=0
HAVE_VLC_QT=0
HAVE_CLOUDBROWSER=0

AC_ARG_WITH([cloudbrowser], AS_HELP_STRING([--with-cloudbrowser]))
AS_IF([test "x$with_cloudbrowser" != "xno"], [
  AS_IF([test "$HAVE_CURL" -eq 1 -a "$HAVE_MICROHTTPD" -eq 1], [
    PKG_CHECK_MODULES([qt], [Qt5Core Qt5Gui Qt5Quick], [
      HAVE_QT=1
      HAVE_CLOUDBROWSER=1
      QT_PATH="$(eval $PKG_CONFIG --variable=exec_prefix Qt5Core)"
      QT_HOST_PATH="$(eval $PKG_CONFIG --variable=host_bins Qt5Core)"
      AC_PATH_PROGS(MOC, [moc-qt5 moc], moc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AC_PATH_PROGS(RCC, [rcc-qt5 rcc], rcc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AC_PATH_PROGS(UIC, [uic-qt5 uic], uic, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AC_DEFINE(WITH_QT)
    ], [
      HAVE_QT=0
      AS_IF([test "x$with_cloudbrowser" = "xyes"], [AC_MSG_ERROR([cloudbrowser requires qt])])
    ])
    PKG_CHECK_MODULES([qtwebview], [Qt5WebView], [
      HAVE_QTWEBVIEW=1
      AC_DEFINE(WITH_QTWEBVIEW)
    ], [HAVE_QTWEBVIEW=0])
    PKG_CHECK_MODULES([libffmpegthumbnailer], [libffmpegthumbnailer], [
      HAVE_THUMBNAILER=1
      AC_DEFINE(WITH_THUMBNAILER)
    ], [HAVE_THUMBNAILER=0])
    PKG_CHECK_MODULES([vlcqt], [libVLCQtQml], [
      HAVE_VLC_QT=1
      AC_DEFINE(WITH_VLC_QT)
    ], [HAVE_VLC_QT=0])
  ], [
    AS_IF([test "x$with_cloudbrowser" = "xyes"], [
      AS_IF([test "$HAVE_CURL" -eq 0], [AC_MSG_ERROR([cloudbrowser requires curl])])
      AS_IF([test "$HAVE_MICROHTTPD" -eq 0], [AC_MSG_ERROR([cloudbrowser requires microhttpd])])
    ])
  ])
])

AM_CONDITIONAL([WITH_QT], [test "$HAVE_QT" -eq 1])
AM_CONDITIONAL([WITH_QTWEBVIEW], [test "$HAVE_QTWEBVIEW" -eq 1])
AM_CONDITIONAL([WITH_THUMBNAILER], [test "$HAVE_THUMBNAILER" -eq 1])
AM_CONDITIONAL([WITH_VLC_QT], [test "$HAVE_VLC_QT" -eq 1])
AM_CONDITIONAL([WITH_CLOUDBROWSER], [test "$HAVE_CLOUDBROWSER" -eq 1])

AC_OUTPUT
