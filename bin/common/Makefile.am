ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(libjsoncpp_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	HttpServer.cpp

noinst_HEADERS = \
	HttpServer.h

libcommon_la_LIBADD = \
	../../src/libcloudstorage.la
