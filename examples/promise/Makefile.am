ACLOCAL_AMFLAGS = -I m4

noinst_PROGRAMS = promise

AM_LDFLAGS = \
	-no-undefined

promise_SOURCES = \
	main.cpp \
	CloudAccess.cpp \
	CloudFactory.cpp \
	CloudEventLoop.cpp \
	LoginPage.cpp

noinst_HEADERS = \
	CloudAccess.h \
	CloudFactory.h \
	CloudEventLoop.h \
	ICloudAccess.h \
	ICloudFactory.h \
	LoginPage.h

promise_LDADD = \
	../../src/libcloudstorage.la \
	../../bin/common/libcommon.la \
	$(libjsoncpp_LIBS)

promise_CXXFLAGS = \
	-I$(top_srcdir)/src/ \
	-I$(top_srcdir)/bin/common \
	$(libjsoncpp_CFLAGS)
