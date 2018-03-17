TEMPLATE = lib
CONFIG += c++11
QT =

DEFINES += \
    WITH_CURL \
    WITH_MEGA \
    WITH_MICROHTTPD \
    WITH_CRYPTOPP \
    CLOUDSTORAGE_LIBRARY

MSVC_TOOLCHAIN_PATH = $$(MSVC_TOOLCHAIN_PATH)

isEmpty(MSVC_TOOLCHAIN_PATH) {
    error(MSVC_TOOLCHAIN_PATH environment variable not set.)
}

winrt {
    DEFINES += HAVE_BOOST_FILESYSTEM_HPP
}

INCLUDEPATH = \
    src \
    $$MSVC_TOOLCHAIN_PATH/include

LIBS += \
    -L$$MSVC_TOOLCHAIN_PATH/lib \
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
    -luser32 \
    -lwinhttp \
    -lws2_32 \
    -ladvapi32

HEADERS += \
    src/ICloudProvider.h \
    src/CloudProvider/AmazonS3.h \
    src/CloudProvider/AnimeZone.h \
    src/CloudProvider/Box.h \
    src/CloudProvider/CloudProvider.h \
    src/CloudProvider/Dropbox.h \
    src/CloudProvider/GoogleDrive.h \
    src/CloudProvider/GooglePhotos.h \
    src/CloudProvider/HubiC.h \
    src/CloudProvider/LocalDrive.h \
    src/CloudProvider/MegaNz.h \
    src/CloudProvider/OneDrive.h \
    src/CloudProvider/PCloud.h \
    src/CloudProvider/WebDav.h \
    src/CloudProvider/YandexDisk.h \
    src/CloudProvider/YouTube.h \
    src/Request/AuthorizeRequest.h \
    src/Request/CreateDirectoryRequest.h \
    src/Request/DeleteItemRequest.h \
    src/Request/DownloadFileRequest.h \
    src/Request/ExchangeCodeRequest.h \
    src/Request/GetItemDataRequest.h \
    src/Request/GetItemRequest.h \
    src/Request/GetItemUrlRequest.h \
    src/Request/HttpCallback.h \
    src/Request/ListDirectoryPageRequest.h \
    src/Request/ListDirectoryRequest.h \
    src/Request/MoveItemRequest.h \
    src/Request/RecursiveRequest.h \
    src/Request/RenameItemRequest.h \
    src/Request/Request.h \
    src/Request/UploadFileRequest.h \
    src/Utility/Auth.h \
    src/Utility/CloudStorage.h \
    src/Utility/CryptoPP.h \
    src/Utility/CurlHttp.h \
    src/Utility/Item.h \
    src/Utility/MicroHttpdServer.h \
    src/Utility/ThreadPool.h \
    src/Utility/Utility.h \
    src/IAuth.h \
    src/ICloudProvider.h \
    src/ICloudStorage.h \
    src/ICrypto.h \
    src/IHttp.h \
    src/IHttpServer.h \
    src/IItem.h \
    src/IRequest.h \
    src/IThreadPool.h

SOURCES += \
    src/CloudProvider/AmazonS3.cpp \
    src/CloudProvider/AnimeZone.cpp \
    src/CloudProvider/Box.cpp \
    src/CloudProvider/CloudProvider.cpp \
    src/CloudProvider/Dropbox.cpp \
    src/CloudProvider/GoogleDrive.cpp \
    src/CloudProvider/GooglePhotos.cpp \
    src/CloudProvider/HubiC.cpp \
    src/CloudProvider/LocalDrive.cpp \
    src/CloudProvider/MegaNz.cpp \
    src/CloudProvider/OneDrive.cpp \
    src/CloudProvider/PCloud.cpp \
    src/CloudProvider/WebDav.cpp \
    src/CloudProvider/YandexDisk.cpp \
    src/CloudProvider/YouTube.cpp \
    src/Request/AuthorizeRequest.cpp \
    src/Request/CreateDirectoryRequest.cpp \
    src/Request/DeleteItemRequest.cpp \
    src/Request/DownloadFileRequest.cpp \
    src/Request/ExchangeCodeRequest.cpp \
    src/Request/GetItemDataRequest.cpp \
    src/Request/GetItemRequest.cpp \
    src/Request/GetItemUrlRequest.cpp \
    src/Request/HttpCallback.cpp \
    src/Request/ListDirectoryPageRequest.cpp \
    src/Request/ListDirectoryRequest.cpp \
    src/Request/MoveItemRequest.cpp \
    src/Request/RecursiveRequest.cpp \
    src/Request/RenameItemRequest.cpp \
    src/Request/Request.cpp \
    src/Request/UploadFileRequest.cpp \
    src/Utility/Auth.cpp \
    src/Utility/CloudStorage.cpp \
    src/Utility/CryptoPP.cpp \
    src/Utility/CurlHttp.cpp \
    src/Utility/Item.cpp \
    src/Utility/MicroHttpdServer.cpp \
    src/Utility/ThreadPool.cpp \
    src/Utility/Utility.cpp
