ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = cloudbrowser
lib_LTLIBRARIES = libcloudbrowser.la

AM_LDFLAGS = \
	-no-undefined

cloudbrowser_SOURCES = src/main.cpp
cloudbrowser_LDADD = libcloudbrowser.la

noinst_HEADERS = \
	src/CloudContext.h \
	src/GenerateThumbnail.h \
	src/AndroidUtility.h \
	src/DesktopUtility.h \
	src/WinRTUtility.h \
	src/FileDialog.h \
	src/Exec.h \
	src/File.h \
	src/IPlatformUtility.h

libcloudbrowser_la_SOURCES = \
	src/CloudContext.cpp \
	src/GenerateThumbnail.cpp \
	src/AndroidUtility.cpp \
	src/DesktopUtility.cpp \
	src/WinRTUtility.cpp \
	src/FileDialog.cpp \
	src/Exec.cpp \
	src/File.cpp

libcloudbrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	-Isrc \
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
endif
if WITH_VLC_QT
libcloudbrowser_la_CXXFLAGS += $(vlcqt_CFLAGS)
libcloudbrowser_la_LIBADD += $(vlcqt_LIBS)
endif

nodist_libcloudbrowser_la_SOURCES = \
	src/CloudContext.moc.cpp \
	src/FileDialog.moc.cpp \
	src/IPlatformUtility.moc.cpp \
	resources.cpp

DEPS_res = \
	qtquickcontrols2.conf \
	config.json \
	qml/main.qml \
	qml/WebView.qml \
	qml/MediaPlayer.qml \
	qml/VlcPlayer.qml \
	qml/QtPlayer.qml \
	qml/ImagePlayer.qml \
	qml/RegisterPage.qml \
	qml/RemoveProvider.qml \
	qml/ProviderEntry.qml \
	qml/ItemPage.qml \
	qml/FileDialog.qml \
	qml/QtFileDialog.qml \
	qml/AndroidFileDialog.qml \
	qml/WinRTFileDialog.qml \
	qml/AboutView.qml \
	resources/cloud.png \
	resources/vlc.png \
	resources/kde.png \
	resources/qt.png \
	resources/vlc-qt.png \
	resources/gnu.png \
	resources/jsoncpp.png \
	resources/curl.png \
	resources/cryptopp.png \
	resources/tinyxml2.png \
	resources/ffmpeg.png \
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
	resources/providers/hubic.png \
	resources/providers/local.png \
	resources/amazons3_login.html \
	resources/default_error.html \
	resources/default_success.html \
	resources/mega_login.html \
	resources/webdav_login.html \
	resources/local_login.html

%.moc.cpp: %.h
	$(AM_V_GEN)$(MOC) -o $@ $<

resources.cpp: resources.qrc $(DEPS_res)
	$(AM_V_GEN)$(RCC) --name resources -o $@ $<

EXTRA_DIST = resources.qrc $(DEPS_res)

BUILT_SOURCES = $(nodist_libcloudbrowser_la_SOURCES)
CLEANFILES = $(BUILT_SOURCES)
