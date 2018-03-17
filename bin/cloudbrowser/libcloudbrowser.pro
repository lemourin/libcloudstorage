TEMPLATE = lib
CONFIG += c++11
QT += quick webview svg network multimedia gui-private

DEFINES += \
    WITH_QTWEBVIEW \
    WITH_THUMBNAILER \
    WITH_VLC_QT \
    CLOUDBROWSER_LIBRARY

MSVC_TOOLCHAIN_PATH = $$(MSVC_TOOLCHAIN_PATH)

isEmpty(MSVC_TOOLCHAIN_PATH) {
    error(MSVC_TOOLCHAIN_PATH environment variable not set.)
}

winrt {
    QMAKE_CXXFLAGS += /ZW \
        /FU\"$$(EXTENSIONSDKDIR)\Microsoft.Advertising.Xaml\10.0\References\CommonConfiguration\neutral\Microsoft.Advertising.winmd\"
}

INCLUDEPATH = \
    src \
    ../../src/ \
    $$MSVC_TOOLCHAIN_PATH/include

LIBS += \
    -L$$MSVC_TOOLCHAIN_PATH/lib \
    -llibcloudstorage \
    -lVLCQtCore \
    -lVLCQtQml \
    -lavformat \
    -lavcodec \
    -lavdevice \
    -lavfilter \
    -lavutil \
    -lswresample \
    -lswscale

HEADERS += \
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
    src/Request/CopyItem.h \
    src/GenerateThumbnail.h \
    src/AndroidUtility.h \
    src/DesktopUtility.h \
    src/WinRTUtility.h \
    src/FileDialog.h \
    src/Exec.h \
    src/File.h \
    src/IPlatformUtility.h

SOURCES += \
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
    src/Request/CopyItem.cpp \
    src/GenerateThumbnail.cpp \
    src/AndroidUtility.cpp \
    src/DesktopUtility.cpp \
    src/WinRTUtility.cpp \
    src/FileDialog.cpp \
    src/Exec.cpp \
    src/File.cpp

RESOURCES += \
    resources.qrc
