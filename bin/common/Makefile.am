ACLOCAL_AMFLAGS = -I m4

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	HttpServer.cpp \
	GenerateThumbnail.cpp

noinst_HEADERS = \
	HttpServer.h \
	GenerateThumbnail.h

libcommon_la_LIBADD = \
	../../src/libcloudstorage.la

libcommon_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(libjsoncpp_CFLAGS)

if WITH_THUMBNAILER
libcommon_la_LIBADD += $(ffmpeg_LIBS)
libcommon_la_CXXFLAGS += $(ffmpeg_CFLAGS)
endif


