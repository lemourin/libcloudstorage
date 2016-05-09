ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libcurl_CXXFLAGS) \
	$(libjsoncpp_CXXFLAGS) \
	$(libmicrohttpd_CXXFLAGS)

lib_LTLIBRARIES = libcloudstorage.la

libcloudstorage_la_SOURCES =

nobase_include_HEADERS =

libcloudstorage_la_LIBADD = \
	$(libcurl_LIBS) \
	$(libjsoncpp_LIBS) \
	$(libmicrohttpd_LIBS)
