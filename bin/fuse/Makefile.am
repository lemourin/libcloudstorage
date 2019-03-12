ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/bin/common \
	$(libjsoncpp_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

bin_PROGRAMS = cloudstorage-fuse

cloudstorage_fuse_SOURCES = \
	FuseCommon.cpp \
	FuseLowLevel.cpp \
	FuseHighLevel.cpp \
	FuseDokan.cpp \
	FileSystem.cpp \
	FuseWinFsp.cpp \
	main.cpp

noinst_HEADERS = \
	FuseCommon.h \
	FuseLowLevel.h \
	FuseHighLevel.h \
	FuseDokan.h \
	FuseWinFsp.h \
	IFileSystem.h \
	FileSystem.h

cloudstorage_fuse_LDADD = \
	../../src/libcloudstorage.la \
	../common/libcommon.la \
	$(libjsoncpp_LIBS)

if WITH_FUSE
AM_CXXFLAGS += $(fuse_CFLAGS)
cloudstorage_fuse_LDADD += $(fuse_LIBS)
endif

if WITH_LEGACY_FUSE
AM_CXXFLAGS += $(fuse2_CFLAGS)
cloudstorage_fuse_LDADD += $(fuse2_LIBS)
endif
