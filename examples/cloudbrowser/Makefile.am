ACLOCAL_AMFLAGS = -I m4

noinst_PROGRAMS = cloudbrowser
noinst_LTLIBRARIES = libcloudbrowser.la

AM_LDFLAGS = \
	-no-undefined

cloudbrowser_SOURCES = main.cpp
cloudbrowser_LDADD = libcloudbrowser.la

libcloudbrowser_la_SOURCES = \
	CloudContext.cpp CloudContext.h \
	Exec.cpp Exec.h \
	File.cpp File.h

libcloudbrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(qt_CFLAGS)

libcloudbrowser_la_LIBADD = \
	$(qt_LIBS) \
	../../src/libcloudstorage.la

if WITH_QTWEBVIEW
libcloudbrowser_la_CXXFLAGS += $(qtwebview_CFLAGS)
libcloudbrowser_la_LIBADD += $(qtwebview_LIBS)
endif
if WITH_THUMBNAILER
libcloudbrowser_la_CXXFLAGS += $(libffmpegthumbnailer_CFLAGS)
libcloudbrowser_la_LIBADD += $(libffmpegthumbnailer_LIBS)
libcloudbrowser_la_SOURCES += \
	GenerateThumbnail.cpp GenerateThumbnail.h
endif
if WITH_VLC_QT
libcloudbrowser_la_CXXFLAGS += $(vlcqt_CFLAGS)
libcloudbrowser_la_LIBADD += $(vlcqt_LIBS)
endif

nodist_libcloudbrowser_la_SOURCES = \
	CloudContext.moc.cpp \
	resources.cpp

DEPS_res = \
	qtquickcontrols2.conf \
	qml/main.qml \
	qml/WebView.qml \
	qml/MediaPlayer.qml \
	qml/VlcPlayer.qml \
	qml/QtPlayer.qml \
	qml/RegisterPage.qml \
	qml/RemoveProvider.qml \
	qml/ProviderEntry.qml \
	qml/ItemPage.qml \
	qml/FileDialog.qml \
	qml/QtFileDialog.qml \
	qml/AndroidFileDialog.qml \
	resources/directory.png \
	resources/file.png \
	resources/image.png \
	resources/video.png \
	resources/audio.png \
	resources/cloud.png \
	resources/providers/google.png \
	resources/providers/onedrive.png \
	resources/providers/dropbox.png \
	resources/providers/amazons3.png \
	resources/providers/box.png \
	resources/providers/pcloud.png \
	resources/providers/mega.png \
	resources/providers/webdav.png \
	resources/providers/youtube.png \
	resources/providers/yandex.png \
	resources/providers/gphotos.png \
	resources/amazons3_login.html \
	resources/default_error.html \
	resources/default_success.html \
	resources/mega_login.html \
	resources/webdav_login.html

%.moc.cpp: %.h
	$(AM_V_GEN)$(MOC) -o $@ $<

resources.cpp: resources.qrc $(DEPS_res)
	$(AM_V_GEN)$(RCC) --name resources -o $@ $<

EXTRA_DIST = resources.qrc $(DEPS_res)

BUILT_SOURCES = $(nodist_libcloudbrowser_la_SOURCES)
CLEANFILES = $(BUILT_SOURCES)
