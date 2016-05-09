AC_INIT([libcloudstorage], [1.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIRS([m4])
LT_INIT

AC_PROG_CXX

AX_CXX_COMPILE_STDCXX_11

PKG_CHECK_MODULES([libcurl], [libcurl])
PKG_CHECK_MODULES([libjsoncpp], [jsoncpp])
PKG_CHECK_MODULES([libmicrohttpd], [libmicrohttpd])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  test/Makefile
])

AC_OUTPUT
