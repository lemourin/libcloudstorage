TEMPLATE = app
CONFIG += c++11
QT += quick webview androidextras svg network

DEFINES += WITH_THUMBNAILER WITH_CURL WITH_CRYPTOPP WITH_MEGA WITH_MICROHTTPD WITH_VLC_QT

ANDROID_TOOLCHAIN_PATH = $$(ANDROID_TOOLCHAIN_PATH)

isEmpty(ANDROID_TOOLCHAIN_PATH) {
    error(ANDROID_TOOLCHAIN_PATH environment variable not set.)
}

QMAKE_CXX = $$ANDROID_TOOLCHAIN_PATH/bin/clang++
QMAKE_CXXFLAGS =
QMAKE_LINK = $$QMAKE_CXX
QMAKE_INCDIR_POST =
QMAKE_LIBS_PRIVATE =

INCLUDEPATH = \
    ../../../src/ \
    $$ANDROID_TOOLCHAIN_PATH/include \
    $$ANDROID_TOOLCHAIN_PATH/include/c++/4.9.x \
    $$ANDROID_TOOLCHAIN_PATH/sysroot/usr/include

LIBS += \
    -L$$ANDROID_TOOLCHAIN_PATH/lib \
    -L$$ANDROID_TOOLCHAIN_PATH/arm-linux-androideabi/lib/ \
    -lc++_shared -lcurl -lmicrohttpd -ljsoncpp -ltinyxml2 -lcryptopp -lmega -lffmpegthumbnailer \
    -lVLCQtQml -lVLCQtCore -lvlcjni

SOURCES += \
    ../main.cpp \
    ../CloudContext.cpp \
    ../GenerateThumbnail.cpp \
    ../Exec.cpp \
    ../FileDialog.cpp \
    ../File.cpp \
    ../AndroidUtility.cpp

SOURCES += \
    ../../../src/Utility/CloudStorage.cpp \
    ../../../src/Utility/Auth.cpp \
    ../../../src/Utility/Item.cpp \
    ../../../src/Utility/FileSystem.cpp \
    ../../../src/Utility/Utility.cpp \
    ../../../src/CloudProvider/CloudProvider.cpp \
    ../../../src/CloudProvider/GoogleDrive.cpp \
    ../../../src/CloudProvider/OneDrive.cpp \
    ../../../src/CloudProvider/Dropbox.cpp \
    ../../../src/CloudProvider/AmazonS3.cpp \
    ../../../src/CloudProvider/Box.cpp \
    ../../../src/CloudProvider/YouTube.cpp \
    ../../../src/CloudProvider/YandexDisk.cpp \
    ../../../src/CloudProvider/WebDav.cpp \
    ../../../src/CloudProvider/PCloud.cpp \
    ../../../src/CloudProvider/HubiC.cpp \
    ../../../src/CloudProvider/GooglePhotos.cpp \
    ../../../src/Request/Request.cpp \
    ../../../src/Request/HttpCallback.cpp \
    ../../../src/Request/AuthorizeRequest.cpp \
    ../../../src/Request/DownloadFileRequest.cpp \
    ../../../src/Request/GetItemRequest.cpp \
    ../../../src/Request/ListDirectoryRequest.cpp \
    ../../../src/Request/ListDirectoryPageRequest.cpp \
    ../../../src/Request/UploadFileRequest.cpp \
    ../../../src/Request/GetItemDataRequest.cpp \
    ../../../src/Request/DeleteItemRequest.cpp \
    ../../../src/Request/CreateDirectoryRequest.cpp \
    ../../../src/Request/MoveItemRequest.cpp \
    ../../../src/Request/RenameItemRequest.cpp \
    ../../../src/Request/ExchangeCodeRequest.cpp \
    ../../../src/Request/GetItemUrlRequest.cpp \
    ../../../src/Request/RecursiveRequest.cpp \
    ../../../src/CloudProvider/MegaNz.cpp \
    ../../../src/Utility/CryptoPP.cpp \
    ../../../src/Utility/CurlHttp.cpp \
    ../../../src/Utility/MicroHttpdServer.cpp

RESOURCES += \
    ../resources.qrc

DISTFILES += \
    AndroidManifest.xml \
    Workaround.qml \
    res/drawable-hdpi/icon.png \
    res/drawable-ldpi/icon.png \
    android/res/drawable-mdpi/icon.png \
    src/org/videolan/cloudbrowser/CloudBrowser.java \
    src/org/videolan/cloudbrowser/AuthView.java

HEADERS += \
    ../CloudContext.h \
    ../FileDialog.h \
    ../File.h \
    ../AndroidUtility.h

ANDROID_PACKAGE_SOURCE_DIR = $$PWD

ANDROID_EXTRA_LIBS = \
    $$ANDROID_TOOLCHAIN_PATH/arm-linux-androideabi/lib/libc++_shared.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libssl.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libcrypto.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libjsoncpp.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libmicrohttpd.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libcurl.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libtinyxml2.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libcares.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libnghttp2.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libsqlite3.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libmega.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libcryptopp.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libavdevice.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libavfilter.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libswscale.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libavformat.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libavcodec.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libswresample.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libavutil.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libpng16.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libffmpegthumbnailer.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libvlcjni.so
