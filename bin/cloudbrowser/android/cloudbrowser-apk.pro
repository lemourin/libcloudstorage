TEMPLATE = app
CONFIG += c++11
QT += quick androidextras svg network

ANDROID_TOOLCHAIN_PATH = $$(ANDROID_TOOLCHAIN_PATH)
ANDROID_ARCH = $$(ANDROID_ARCH)

isEmpty(ANDROID_TOOLCHAIN_PATH) {
    error(ANDROID_TOOLCHAIN_PATH environment variable not set.)
}

isEmpty(ANDROID_ARCH) {
    error(ANDROID_ARCH environment variable not set.)
}

QMAKE_CXX = $$ANDROID_TOOLCHAIN_PATH/bin/clang++
QMAKE_CXXFLAGS =
QMAKE_LFLAGS = --no-undefined
QMAKE_LINK = $$QMAKE_CXX
QMAKE_INCDIR_POST =
QMAKE_LIBS_PRIVATE =

INCLUDEPATH = ../src

LIBS += \
    -L$$(ANDROID_NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/$$ANDROID_ARCH \
    -L$$ANDROID_TOOLCHAIN_PATH/lib \
    -lc++ \
    $$ANDROID_TOOLCHAIN_PATH/lib/libcloudbrowser.a \
    $$ANDROID_TOOLCHAIN_PATH/lib/libcloudstorage.a \
    -lcurl -lmicrohttpd -ljsoncpp -ltinyxml2 -lcryptopp -lmega \
    -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale \
    -lboost_filesystem -lboost_system \
    -lVLCQtQml -lVLCQtCore -lvlcjni

SOURCES += ../src/main.cpp

DISTFILES += \
    AndroidManifest.xml \
    Workaround.qml \
    src/org/videolan/cloudbrowser/CloudBrowser.java \
    src/org/videolan/cloudbrowser/AuthView.java \
    src/org/videolan/cloudbrowser/NotificationHelper.java \
    src/org/videolan/cloudbrowser/NotificationService.java \
    src/org/videolan/cloudbrowser/FileInput.java \
    src/org/videolan/cloudbrowser/Utility.java \
    src/org/videolan/cloudbrowser/FileOutput.java \
    build.gradle \
    project.properties

ANDROID_PACKAGE_SOURCE_DIR = $$PWD

ANDROID_EXTRA_LIBS = \
    $$ANDROID_TOOLCHAIN_PATH/$$(ANDROID_NDK_TOOLS_PREFIX)/lib/libc++_shared.so \
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
    $$ANDROID_TOOLCHAIN_PATH/lib/libboost_system.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libboost_filesystem.so \
    $$ANDROID_TOOLCHAIN_PATH/lib/libvlcjni.so
