ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = cloudbrowser
lib_LTLIBRARIES = libcloudbrowser.la

AM_LDFLAGS = \
	-no-undefined

cloudbrowser_SOURCES = main.cpp resources.cpp
cloudbrowser_LDADD = libcloudbrowser.la

libcloudbrowser_la_SOURCES = \
	Window.cpp Window.h \
	Callback.cpp Callback.h \
	MediaPlayer.cpp MediaPlayer.h \
	MockProvider.cpp MockProvider.h \
	Exec.cpp Exec.h

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
if WITH_QTWEBENGINE
libcloudbrowser_la_CXXFLAGS += \
	$(qtwebengine_CFLAGS)
libcloudbrowser_la_LIBADD += \
	$(qtwebengine_LIBS)
endif
if WITH_THUMBNAILER
libcloudbrowser_la_CXXFLAGS += $(libffmpegthumbnailer_CFLAGS)
libcloudbrowser_la_LIBADD += $(libffmpegthumbnailer_LIBS)
libcloudbrowser_la_SOURCES += \
	GenerateThumbnail.cpp GenerateThumbnail.h
endif
if WITH_CRYPTO
libcloudbrowser_la_CXXFLAGS += \
	$(libcrypto_CFLAGS)
libcloudbrowser_la_LIBADD += \
	$(libcrypto_LIBS)
endif

nodist_libcloudbrowser_la_SOURCES = \
	Window.moc.cpp \
	MediaPlayer.moc.cpp

DEPS_res = \
	qml/main.qml \
	qml/WebView.qml \
	qml/WebKit.qml \
	qml/MediaPlayer.qml \
	qml/ActionButton.qml \
	qml/ActionTextButton.qml \
	resources/directory.png \
	resources/file.png \
	resources/image.png \
	resources/video.png \
	resources/audio.png \
	resources/cloud.png \
	resources/amazons3_login.html \
	resources/amazons3_success.html \
	resources/default_error.html \
	resources/default_success.html \
	resources/mega_login.html \
	resources/mega_success.html \
	resources/webdav_login.html \
	resources/webdav_success.html

%.moc.cpp: %.h
	$(MOC) -o $@ $<

resources.cpp: qml.qrc $(DEPS_res)
	$(RCC) -o $@ $<

EXTRA_DIST = qml.qrc $(DEPS_res)

BUILT_SOURCES = $(nodist_cloudbrowser_SOURCES)
