ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(libjsoncpp_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

bin_PROGRAMS = cloudstorage-fuse

cloudstorage_fuse_SOURCES = \
	../cloudbrowser/src/HttpServer.cpp \
	FuseCommon.cpp \
	FuseLowLevel.cpp \
	FuseHighLevel.cpp \
	FuseDokan.cpp \
	FileSystem.cpp \
	main.cpp

noinst_HEADERS = \
	../cloudbrowser/src/HttpServer.h \
	FuseCommon.h \
	FuseLowLevel.h \
	FuseHighLevel.h \
	FuseDokan.h \
	IFileSystem.h \
	FileSystem.h

cloudstorage_fuse_LDADD = \
	../../src/libcloudstorage.la \
	$(libjsoncpp_LIBS)

if WITH_FUSE
AM_CXXFLAGS += $(fuse_CFLAGS)
cloudstorage_fuse_LDADD += $(fuse_LIBS)
endif

if WITH_LEGACY_FUSE
AM_CXXFLAGS += $(fuse2_CFLAGS)
cloudstorage_fuse_LDADD += $(fuse2_LIBS)
endif
