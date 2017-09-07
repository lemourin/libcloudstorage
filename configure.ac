AC_INIT([libcloudstorage], [1.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
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

AC_ARG_WITH([cryptopp], AS_HELP_STRING([--with-cryptopp]))
AM_CONDITIONAL([WITH_CRYPTOPP], [test "x$with_cryptopp" = "xyes"])
AS_IF([test "x$with_cryptopp" = "xyes"], [
  PKG_CHECK_MODULES([libcryptopp], [libcrypto++])
  AC_DEFINE(WITH_CRYPTOPP)
])

AC_ARG_WITH([curl], AS_HELP_STRING([--with-curl]))
AM_CONDITIONAL([WITH_CURL], [test "x$with_curl" = "xyes"])
AS_IF([test "x$with_curl" = "xyes"], [
  PKG_CHECK_MODULES([libcurl], [libcurl])
  AC_DEFINE(WITH_CURL)
])

AC_ARG_WITH([microhttpd], AS_HELP_STRING([--with-microhttpd]))
AM_CONDITIONAL([WITH_MICROHTTPD], [test "x$with_microhttpd" = "xyes"])
AS_IF([test "x$with_microhttpd" = "xyes"], [
  PKG_CHECK_MODULES([libmicrohttpd], [libmicrohttpd])
  AC_DEFINE(WITH_MICROHTTPD)
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
  examples/Makefile
  examples/cloudbrowser/Makefile
  examples/commandlinebrowser/Makefile
])

SOCKET_LIBS=""
AS_IF([test "${SYS}" = "mingw32"], [
  SOCKET_LIBS="-lws2_32"
])
AC_SUBST(SOCKET_LIBS)

AC_ARG_WITH([examples], AS_HELP_STRING([--with-examples]))
AM_CONDITIONAL([EXAMPLES], [test "x$with_examples" = "xyes"])

AC_ARG_WITH([vlc], AS_HELP_STRING([--with-vlc]))
AM_CONDITIONAL([WITH_VLC], [test "x$with_vlc" = "xyes"])

AC_ARG_WITH([qtmultimediawidgets], AS_HELP_STRING([--with-qtmultimediawidgets]))
AM_CONDITIONAL([WITH_QTMULTIMEDIAWIDGETS], [test "x$with_qtmultimediawidgets" = "xyes"])

AC_ARG_WITH([qmlplayer], AS_HELP_STRING([--with-qmlplayer]))
AM_CONDITIONAL([WITH_QMLPLAYER], [test "x$with_qmlplayer" = "xyes"])

HAVE_QTWEBENGINE=0
HAVE_THUMBNAILER=0

AS_IF([test "x$with_examples" = "xyes"], [
  PKG_CHECK_MODULES([qt], [Qt5Core Qt5Gui Qt5Quick], [
    QT_PATH="$(eval $PKG_CONFIG --variable=exec_prefix Qt5Core)"
    QT_HOST_PATH="$(eval $PKG_CONFIG --variable=host_bins Qt5Core)"
    AC_PATH_PROGS(MOC, [moc-qt5 moc], moc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
    AC_PATH_PROGS(RCC, [rcc-qt5 rcc], rcc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
    AC_PATH_PROGS(UIC, [uic-qt5 uic], uic, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
  ])
  PKG_CHECK_MODULES([qtwebengine], [Qt5WebEngine], [
    HAVE_QTWEBENGINE=1
    AC_DEFINE(WITH_QTWEBENGINE)
  ], [HAVE_QTWEBENGINE=0])
  PKG_CHECK_MODULES([libffmpegthumbnailer], [libffmpegthumbnailer], [
    HAVE_THUMBNAILER=1
    AC_DEFINE(WITH_THUMBNAILER)
  ], [HAVE_THUMBNAILER=0])
  AS_IF([test "x$with_vlc" = "xyes"], [
    PKG_CHECK_MODULES([libvlcpp], [libvlcpp])
    PKG_CHECK_MODULES([libvlc], [libvlc])
    PKG_CHECK_MODULES([qtwidgets], [Qt5Widgets])
    AC_DEFINE(WITH_VLC)
  ], [test "x$with_qtmultimediawidgets" = "xyes"], [
    PKG_CHECK_MODULES([qtmultimediawidgets], [Qt5Widgets Qt5Multimedia Qt5MultimediaWidgets])
    AC_DEFINE(WITH_QTMULTIMEDIAWIDGETS)
  ], [
    PKG_CHECK_MODULES([qtmultimedia], [Qt5Multimedia])
    AC_DEFINE(WITH_QMLPLAYER)
  ])
])

AM_CONDITIONAL([WITH_QTWEBENGINE], [test "$HAVE_QTWEBENGINE" -eq 1])
AM_CONDITIONAL([WITH_THUMBNAILER], [test "$HAVE_THUMBNAILER" -eq 1])

AC_OUTPUT
