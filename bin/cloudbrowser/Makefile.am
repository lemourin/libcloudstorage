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
	src/Request/CopyItem.h \
	src/AndroidUtility.h \
	src/DesktopUtility.h \
	src/WinRTUtility.h \
	src/FileDialog.h \
	src/Exec.h \
	src/File.h \
	src/IPlatformUtility.h \
	src/MpvPlayer.h

libcloudbrowser_la_SOURCES = \
	src/CloudContext.cpp \
	src/CloudItem.cpp \
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
	src/Request/CopyItem.cpp \
	src/AndroidUtility.cpp \
	src/DesktopUtility.cpp \
	src/WinRTUtility.cpp \
	src/FileDialog.cpp \
	src/Exec.cpp \
	src/File.cpp \
	src/MpvPlayer.cpp

libcloudbrowser_la_CXXFLAGS = \
	-I$(top_srcdir)/bin/cloudbrowser/src \
	-I$(top_srcdir)/src \
	$(libjsoncpp_CFLAGS) \
	$(qt_CFLAGS)

libcloudbrowser_la_LIBADD = \
	$(libjsoncpp_LIBS) \
	$(qt_LIBS) \
	../../src/libcloudstorage.la

if WITH_QTANDROID
libcloudbrowser_la_CXXFLAGS += $(qtandroid_CFLAGS)
libcloudbrowser_la_LIBADD += $(qtandroid_LIBS)
endif
if ANDROID
libcloudbrowser_la_LIBADD += -llog
endif
if WITH_QTWEBVIEW
libcloudbrowser_la_CXXFLAGS += $(qtwebview_CFLAGS)
libcloudbrowser_la_LIBADD += $(qtwebview_LIBS)
endif
if WITH_THUMBNAILER
libcloudbrowser_la_CXXFLAGS += $(ffmpeg_CFLAGS)
libcloudbrowser_la_LIBADD += $(ffmpeg_LIBS)
endif
if WITH_VLC_QT
libcloudbrowser_la_CXXFLAGS += $(vlcqt_CFLAGS)
libcloudbrowser_la_LIBADD += $(vlcqt_LIBS)
endif
if WITH_MPV
libcloudbrowser_la_CXXFLAGS += $(mpv_CFLAGS) -DWITH_MPV
libcloudbrowser_la_LIBADD += $(mpv_LIBS)
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
	src/Request/CopyItem.moc.cpp \
	src/FileDialog.moc.cpp \
	src/IPlatformUtility.moc.cpp \
	src/MpvPlayer.moc.cpp \
	resources.cpp

DEPS_res = \
	qtquickcontrols2.conf \
	config.json \
	qml/main.qml \
	qml/WebView.qml \
	qml/MediaPlayer.qml \
	qml/VlcPlayer.qml \
	qml/QtPlayer.qml \
	qml/MpvPlayer.qml \
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
	resources/providers/localwinrt.png \
	resources/providers/animezone.png \
	resources/providers/4shared.png \
	resources/amazons3_login.html \
	resources/default_error.html \
	resources/default_success.html \
	resources/mega_login.html \
	resources/webdav_login.html \
	resources/local_login.html \
	resources/localwinrt_login.html \
	resources/animezone_login.html \
	resources/4shared_login.html \
	resources/bootstrap.min.css \
	resources/bootstrap.min.js \
	resources/jquery.min.js \
	resources/style.min.css \
	resources/url.min.js \
	resources/vlc-blue.png \
	resources/mpv.png \
	resources/icons/material/hdpi/view-fullscreen.svg \
	resources/icons/material/hdpi/media-playback-pause.svg \
	resources/icons/material/hdpi/audio-volume-low.svg \
	resources/icons/material/hdpi/edit-paste-style.svg \
	resources/icons/material/hdpi/go-previous-symbolic-rtl.svg \
	resources/icons/material/hdpi/view-list-icons.svg \
	resources/icons/material/hdpi/package-supported.svg \
	resources/icons/material/hdpi/mail-reply-sender.svg \
	resources/icons/material/hdpi/media-record-symbolic.svg \
	resources/icons/material/hdpi/go-down.svg \
	resources/icons/material/hdpi/folder-move.svg \
	resources/icons/material/hdpi/help-about.svg \
	resources/icons/material/hdpi/settings-configure.svg \
	resources/icons/material/hdpi/applications-graphics.svg \
	resources/icons/material/hdpi/go-previous-symbolic.svg \
	resources/icons/material/hdpi/configure.svg \
	resources/icons/material/hdpi/image.svg \
	resources/icons/material/hdpi/edit-add.svg \
	resources/icons/material/hdpi/audio-volume-medium.svg \
	resources/icons/material/hdpi/handle-left.svg \
	resources/icons/material/hdpi/document-save.svg \
	resources/icons/material/hdpi/media-playlist-shuffle.svg \
	resources/icons/material/hdpi/go-next-symbolic-rtl.svg \
	resources/icons/material/hdpi/media-playback-start.svg \
	resources/icons/material/hdpi/go-next-symbolic.svg \
	resources/icons/material/hdpi/audio-volume-high.svg \
	resources/icons/material/hdpi/folder.svg \
	resources/icons/material/hdpi/overflow-menu.svg \
	resources/icons/material/hdpi/bookmarks.svg \
	resources/icons/material/hdpi/folder-sync.svg \
	resources/icons/material/hdpi/edit-cut.svg \
	resources/icons/material/hdpi/go-previous.svg \
	resources/icons/material/hdpi/media-album-track.svg \
	resources/icons/material/hdpi/audio-x-generic.svg \
	resources/icons/material/hdpi/media-view-subtitles-symbolic.svg \
	resources/icons/material/hdpi/media-skip-forward.svg \
	resources/icons/material/hdpi/handle-right.svg \
	resources/icons/material/hdpi/view-list-details.svg \
	resources/icons/material/hdpi/dialog-cancel.svg \
	resources/icons/material/hdpi/folder-new.svg \
	resources/icons/material/hdpi/audio-volume-muted.svg \
	resources/icons/material/hdpi/player-volume.svg \
	resources/icons/material/hdpi/list-add.svg \
	resources/icons/material/hdpi/go-up.svg \
	resources/icons/material/hdpi/document-open-remote.svg \
	resources/icons/material/hdpi/go-next.svg \
	resources/icons/material/hdpi/dialog-apply.svg \
	resources/icons/material/hdpi/document-edit.svg \
	resources/icons/material/hdpi/video.svg \
	resources/icons/material/hdpi/gtk-file.svg \
	resources/icons/material/hdpi/document-decrypt.svg \
	resources/icons/material/hdpi/dialog-close.svg \
	resources/icons/material/hdpi/edit-delete.svg \
	resources/icons/material/hdpi/multimedia-player.svg \
	resources/icons/material/index.theme

%.moc.cpp: %.h
	$(AM_V_GEN)$(MOC) -o $@ $< $(libcloudbrowser_la_CXXFLAGS)

resources.cpp: resources.qrc $(DEPS_res)
	$(AM_V_GEN)$(RCC) --name resources -o $@ $<

EXTRA_DIST = resources.qrc $(DEPS_res)

BUILT_SOURCES = $(nodist_libcloudbrowser_la_SOURCES)
CLEANFILES = $(BUILT_SOURCES)
