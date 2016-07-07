AC_INIT([libcloudstorage], [1.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIRS([m4])
AC_USE_SYSTEM_EXTENSIONS
AC_LANG(C++)

LT_INIT([win32-dll shared disable-static])

AC_PROG_CXX

AX_CXX_COMPILE_STDCXX_11

PKG_CHECK_MODULES([libcurl], [libcurl])
PKG_CHECK_MODULES([libjsoncpp], [jsoncpp])
PKG_CHECK_MODULES([libmicrohttpd], [libmicrohttpd])

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
  src/Makefile
  test/Makefile
  examples/Makefile
  examples/cloudbrowser/Makefile
])

SOCKET_LIBS=""
AS_IF([test "${SYS}" = "mingw32"], [
  SOCKET_LIBS="-lws2_32"
])
AC_SUBST(SOCKET_LIBS)

AC_ARG_WITH([examples], AS_HELP_STRING([--with-examples]))
AM_CONDITIONAL([EXAMPLES], [test "x$with_examples" = "xyes"])

AS_IF([test "x$with_examples" = "xyes"], [
  PKG_CHECK_MODULES(QT, [Qt5Core Qt5Gui Qt5Quick], [
    QT_PATH="$(eval $PKG_CONFIG --variable=exec_prefix Qt5Core)"
    QT_HOST_PATH="$(eval $PKG_CONFIG --variable=host_bins Qt5Core)"
    AC_PATH_PROGS(MOC, [moc-qt5 moc], moc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
    AC_PATH_PROGS(RCC, [rcc-qt5 rcc], rcc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
    AC_PATH_PROGS(UIC, [uic-qt5 uic], uic, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
  ])
  PKG_CHECK_MODULES(LIBVLCPP, [libvlcpp])
  PKG_CHECK_MODULES(LIBVLC, [libvlc])
])

AC_OUTPUT
