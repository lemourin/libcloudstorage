ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(fuse_CFLAGS) \
	$(libjsoncpp_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

bin_PROGRAMS = libcloudstorage-fuse

libcloudstorage_fuse_SOURCES = \
	main.cpp \
	FuseLowLevel.h FuseLowLevel.cpp \
	FuseHighLevel.h FuseHighLevel.cpp	

libcloudstorage_fuse_LDADD = \
	../../src/libcloudstorage.la \
	$(fuse_LIBS) \
	$(libjsoncpp_LIBS)

