ACLOCAL_AMFLAGS = -I m4

noinst_PROGRAMS = cinterface

AM_LDFLAGS = \
	-no-undefined

cinterface_SOURCES = main.c
cinterface_LDADD = \
	../../src/libcloudstorage.la

cinterface_CFLAGS = \
	-I$(top_srcdir)/src/C
