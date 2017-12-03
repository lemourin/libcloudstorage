TEMPLATE = app
CONFIG += c++11
QT += quick webview svg network multimedia

DEFINES += \
    WITH_MICROHTTPD \
    WITH_CURL \
    WITH_CRYPTOPP \
    WITH_QTWEBVIEW \
    WITH_MEGA \
    WITH_THUMBNAILER \
    WITH_VLC_QT

MSVC_TOOLCHAIN_PATH = $$(MSVC_TOOLCHAIN_PATH)

isEmpty(MSVC_TOOLCHAIN_PATH) {
    error(MSVC_TOOLCHAIN_PATH environment variable not set.)
}

INCLUDEPATH = \
    ../../../src/ \
    $$MSVC_TOOLCHAIN_PATH/include

LIBS += \
    -L$$MSVC_TOOLCHAIN_PATH/lib \
    -lVLCQtCore \
    -lVLCQtQml \
    -llibcurl \
    -llibmicrohttpd-dll \
    -ljsoncpp \
    -ltinyxml2 \
    -lmega \
    -lcares \
    -lcryptlib \
    -lshlwapi \
    -lssleay32 \
    -llibeay32 \
    -lffmpegthumbnailer \
    -llibpng16 \
    -lavformat \
    -lavcodec \
    -lavdevice \
    -lavfilter \
    -lavutil \
    -lswresample \
    -lswscale \
    -lgdi32 \
    -luser32 \
    -lwinhttp \
    -lws2_32 \
    -ladvapi32

SOURCES += \
    ../main.cpp \
    ../CloudContext.cpp \
    ../GenerateThumbnail.cpp \
    ../Exec.cpp \
    ../File.cpp

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

HEADERS += \
    ../CloudContext.h \
    ../File.h

DISTFILES += \
    assets/logo_44x44.png \
    assets/logo_150x150.png \
    assets/logo_620x300.png \
    assets/logo_store.png \
    Package.appxmanifest
