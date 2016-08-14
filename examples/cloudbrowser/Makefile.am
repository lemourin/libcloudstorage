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
	MediaPlayer.cpp MediaPlayer.h \
	MockProvider.cpp MockProvider.h \
	main.cpp

libcloudbrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(qt_CFLAGS)

libcloudbrowser_la_LIBADD = \
	../../src/libcloudstorage.la \
	$(qt_LIBS)

if WITH_VLC
libcloudbrowser_la_CXXFLAGS += \
	$(libvlcpp_CFLAGS) \
	$(libvlc_CFLAGS) \
	$(qtwidgets_CFLAGS)
libcloudbrowser_la_LIBADD += \
	$(libvlcpp_LIBS) \
	$(libvlc_LIBS) \
	$(qtwidgets_LIBS)
endif
if WITH_QTMULTIMEDIAWIDGETS
libcloudbrowser_la_CXXFLAGS += \
	$(qtmultimediawidgets_CFLAGS)
libcloudbrowser_la_LIBADD += \
	$(qtmultimediawidgets_LIBS)
endif
if WITH_QMLPLAYER
libcloudbrowser_la_CXXFLAGS += \
	$(qtmultimedia_CFLAGS)
libcloudbrowser_la_LIBADD += \
	$(qtmultimedia_LIBS)
endif

nodist_libcloudbrowser_la_SOURCES = \
	Window.moc.cpp \
	MediaPlayer.moc.cpp \
	resources.cpp

DEPS_res = \
	qml/main.qml \
	qml/WebView.qml \
	qml/MediaPlayer.qml \
	qml/ActionButton.qml \
	qml/ActionTextButton.qml \
	resources/directory.png \
	resources/file.png \
	resources/image.png \
	resources/video.png \
	resources/audio.png \
	resources/cloud.png

%.moc.cpp: %.h
	$(MOC) -o $@ $<

resources.cpp: qml.qrc $(DEPS_res)
	$(RCC) -o $@ $<

EXTRA_DIST = qml.qrc $(DEPS_res)

BUILT_SOURCES = $(nodist_cloudbrowser_SOURCES)
