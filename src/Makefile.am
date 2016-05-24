ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libcurl_CXXFLAGS) \
	$(libjsoncpp_CXXFLAGS) \
	$(libmicrohttpd_CXXFLAGS)

lib_LTLIBRARIES = libcloudstorage.la

libcloudstorage_la_SOURCES = \
	Auth.cpp \
	Item.cpp \
	CloudProvider.cpp

noinst_HEADERS = \
	Utility.h \
	IAuth.h \
	Auth.h \
	Item.h \
	CloudProvider.h

include_HEADERS = \
	IItem.h \
	ICloudProvider.h

libcloudstorage_la_LIBADD = \
	$(libcurl_LIBS) \
	$(libjsoncpp_LIBS) \
	$(libmicrohttpd_LIBS)
