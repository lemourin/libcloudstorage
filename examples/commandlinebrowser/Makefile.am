ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = commandlinebrowser
lib_LTLIBRARIES = libcommandlinebrowser.la

AM_LDFLAGS = \
	-no-undefined

commandlinebrowser_SOURCES = 
commandlinebrowser_LDADD = libcommandlinebrowser.la

libcommandlinebrowser_la_SOURCES = \
	main.cpp

libcommandlinebrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/src

libcommandlinebrowser_la_LIBADD = \
	../../src/libcloudstorage.la
