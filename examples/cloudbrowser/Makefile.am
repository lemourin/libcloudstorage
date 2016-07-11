ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = cloudbrowser
lib_LTLIBRARIES = libcloudbrowser.la

AM_LDFLAGS = \
	-no-undefined

cloudbrowser_SOURCES = 
cloudbrowser_LDADD = libcloudbrowser.la

libcloudbrowser_la_SOURCES = \
	Window.cpp Window.h \
	Callback.cpp Callback.h \
	MockProvider.cpp MockProvider.h \
	main.cpp

libcloudbrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(QT_CFLAGS) \
	$(LIBVLCPP_CFLAGS) \
	$(LIBVLC_CFLAGS)

libcloudbrowser_la_LIBADD = \
	../../src/libcloudstorage.la \
	$(QT_LIBS) \
	$(LIBVLCPP_LIBS) \
	$(LIBVLC_LIBS)

nodist_libcloudbrowser_la_SOURCES = \
	Window.moc.cpp \
	Callback.moc.cpp \
	resources.cpp

DEPS_res = \
	main.qml \
	resources/directory.png \
	resources/file.png

%.moc.cpp: %.h
	$(MOC) -o $@ $<

resources.cpp: qml.qrc $(DEPS_res)
	$(RCC) -o $@ $<

EXTRA_DIST = qml.qrc $(DEPS_res)

BUILT_SOURCES = $(nodist_cloudbrowser_SOURCES)
