ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = -I$(top_srcdir)/src

bin_PROGRAMS = main
main_SOURCES = main.cpp

main_LDADD = \
  ../src/libcloudstorage.la \
  $(libjsoncpp_LIBS)
