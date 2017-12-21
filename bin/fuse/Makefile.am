ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(libjsoncpp_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

bin_PROGRAMS = libcloudstorage-fuse

libcloudstorage_fuse_SOURCES = \
	FuseCommon.h FuseCommon.cpp \
	FuseLowLevel.h FuseLowLevel.cpp \
	FuseHighLevel.h FuseHighLevel.cpp \
	main.cpp

libcloudstorage_fuse_LDADD = \
	../../src/libcloudstorage.la \
	$(libjsoncpp_LIBS)

if WITH_FUSE
AM_CXXFLAGS += $(fuse_CFLAGS)
libcloudstorage_fuse_LDADD += $(fuse_LIBS)
endif

if WITH_LEGACY_FUSE
AM_CXXFLAGS += $(fuse2_CFLAGS)
libcloudstorage_fuse_LDADD += $(fuse2_LIBS)
endif
