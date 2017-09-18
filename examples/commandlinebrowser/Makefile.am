ACLOCAL_AMFLAGS = -I m4

noinst_PROGRAMS = commandlinebrowser

AM_LDFLAGS = \
	-no-undefined

commandlinebrowser_SOURCES = main.cpp
commandlinebrowser_LDADD = ../../src/libcloudstorage.la

commandlinebrowser_CXXFLAGS = \
	-I$(top_srcdir)/src
