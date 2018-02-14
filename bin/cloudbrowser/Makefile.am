ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = cloudbrowser
lib_LTLIBRARIES = libcloudbrowser.la

AM_LDFLAGS = \
	-no-undefined

cloudbrowser_SOURCES = src/main.cpp
cloudbrowser_LDADD = libcloudbrowser.la

noinst_HEADERS = \
	src/CloudContext.h \
	src/CloudItem.h \
	src/HttpServer.h \
	src/Request/CloudRequest.h \
	src/Request/ListDirectory.h \
	src/Request/GetThumbnail.h \
	src/Request/GetUrl.h \
	src/Request/CreateDirectory.h \
	src/Request/DeleteItem.h \
	src/Request/RenameItem.h \
	src/Request/MoveItem.h \
	src/Request/UploadItem.h \
	src/Request/DownloadItem.h \
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
	src/CloudItem.cpp \
	src/HttpServer.cpp \
	src/Request/CloudRequest.cpp \
	src/Request/ListDirectory.cpp \
	src/Request/GetThumbnail.cpp \
	src/Request/GetUrl.cpp \
	src/Request/CreateDirectory.cpp \
	src/Request/DeleteItem.cpp \
	src/Request/RenameItem.cpp \
	src/Request/MoveItem.cpp \
	src/Request/UploadItem.cpp \
	src/Request/DownloadItem.cpp \
	src/GenerateThumbnail.cpp \
	src/AndroidUtility.cpp \
	src/DesktopUtility.cpp \
	src/WinRTUtility.cpp \
	src/FileDialog.cpp \
	src/Exec.cpp \
	src/File.cpp

libcloudbrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/bin/cloudbrowser/src \
	-I$(top_srcdir)/src \
	$(qt_CFLAGS)

libcloudbrowser_la_LIBADD = \
	$(qt_LIBS) \
	../../src/libcloudstorage.la

if WITH_QTANDROID
libcloudbrowser_la_CXXFLAGS += $(qtandroid_CFLAGS)
libcloudbrowser_la_LIBADD += $(qtandroid_LIBS)
endif
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
	src/CloudItem.moc.cpp \
	src/Request/CloudRequest.moc.cpp \
	src/Request/ListDirectory.moc.cpp \
	src/Request/GetThumbnail.moc.cpp \
	src/Request/GetUrl.moc.cpp \
	src/Request/CreateDirectory.moc.cpp \
	src/Request/DeleteItem.moc.cpp \
	src/Request/RenameItem.moc.cpp \
	src/Request/MoveItem.moc.cpp \
	src/Request/UploadItem.moc.cpp \
	src/Request/DownloadItem.moc.cpp \
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
