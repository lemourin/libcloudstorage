TEMPLATE = app
CONFIG += c++11
QT += quick webview svg network multimedia gui-private

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

winrt {
    QMAKE_CXXFLAGS += /ZW \
        /FU\"$$(EXTENSIONSDKDIR)\Microsoft.Advertising.Xaml\10.0\References\CommonConfiguration\neutral\Microsoft.Advertising.winmd\"
} else {
    DEFINES += HAVE_BOOST_FILESYSTEM_HPP
}

INCLUDEPATH = \
    ../src \
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

HEADERS += \
    ../src/CloudContext.h \
    ../src/CloudItem.h \
    ../src/HttpServer.h \
    ../src/Request/CloudRequest.h \
    ../src/Request/ListDirectory.h \
    ../src/Request/GetThumbnail.h \
    ../src/Request/GetUrl.h \
    ../src/Request/CreateDirectory.h \
    ../src/Request/DeleteItem.h \
    ../src/Request/RenameItem.h \
    ../src/Request/MoveItem.h \
    ../src/Request/UploadItem.h \
    ../src/Request/DownloadItem.h \
    ../src/GenerateThumbnail.h \
    ../src/AndroidUtility.h \
    ../src/DesktopUtility.h \
    ../src/WinRTUtility.h \
    ../src/FileDialog.h \
    ../src/Exec.h \
    ../src/File.h \
    ../src/IPlatformUtility.h

SOURCES += \
    ../src/CloudContext.cpp \
    ../src/CloudItem.cpp \
    ../src/HttpServer.cpp \
    ../src/Request/CloudRequest.cpp \
    ../src/Request/ListDirectory.cpp \
    ../src/Request/GetThumbnail.cpp \
    ../src/Request/GetUrl.cpp \
    ../src/Request/CreateDirectory.cpp \
    ../src/Request/DeleteItem.cpp \
    ../src/Request/RenameItem.cpp \
    ../src/Request/MoveItem.cpp \
    ../src/Request/UploadItem.cpp \
    ../src/Request/DownloadItem.cpp \
    ../src/GenerateThumbnail.cpp \
    ../src/AndroidUtility.cpp \
    ../src/DesktopUtility.cpp \
    ../src/WinRTUtility.cpp \
    ../src/FileDialog.cpp \
    ../src/Exec.cpp \
    ../src/File.cpp

SOURCES += \
    ../src/main.cpp \
    ../../../src/Utility/CloudStorage.cpp \
    ../../../src/Utility/Auth.cpp \
    ../../../src/Utility/Item.cpp \
    ../../../src/Utility/Utility.cpp \
    ../../../src/Utility/ThreadPool.cpp \
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
    ../../../src/CloudProvider/LocalDrive.cpp \
    ../../../src/CloudProvider/AnimeZone.cpp \
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
    assets/logo_44x44.png \
    assets/logo_150x150.png \
    assets/logo_620x300.png \
    assets/logo_store.png

WINRT_MANIFEST.name = "Cloud Browser"
WINRT_MANIFEST.background = lightBlue
WINRT_MANIFEST.logo_store = assets/logo_store.png
WINRT_MANIFEST.logo_small = assets/logo_44x44.png
WINRT_MANIFEST.logo_150x150 = assets/logo_150x150.png
WINRT_MANIFEST.logo_620x300 = assets/logo_620x300.png
WINRT_MANIFEST.capabilities = \
    codeGeneration internetClient internetClientServer \
    privateNetworkClientServer
