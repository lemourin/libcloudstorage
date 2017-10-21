ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libjsoncpp_CFLAGS) \
	-I$(top_srcdir)/test/googletest/googletest/include \
	-I$(top_srcdir)/test/googletest/googlemock/include \
	-I$(top_srcdir)/src

check_LTLIBRARIES = libgtest.la libgmock.la

libgtest_la_SOURCES = googletest/googletest/src/gtest-all.cc

libgtest_la_CXXFLAGS = \
	-I$(top_srcdir)/test/googletest/googletest \
	-I$(top_srcdir)/test/googletest/googletest/include

libgmock_la_SOURCES = googletest/googlemock/src/gmock-all.cc

libgmock_la_CXXFLAGS = \
	$(libgtest_la_CXXFLAGS) \
	-I$(top_srcdir)/test/googletest/googlemock \
	-I$(top_srcdir)/test/googletest/googlemock/include

check_PROGRAMS = main

main_SOURCES = \
	main.cpp \
	CloudProvider/CloudProviderTest.cpp \
	CloudProvider/GoogleDriveTest.cpp

check_HEADERS = \
	Utility/HttpMock.h \
	Utility/HttpServerMock.h

main_LDFLAGS = -pthread

main_LDADD = \
	../src/libcloudstorage.la \
	libgtest.la \
	libgmock.la \
	$(libjsoncpp_LIBS)

TESTS = main
EXTRA_DIST = googletest
