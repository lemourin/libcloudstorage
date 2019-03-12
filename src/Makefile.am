ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libjsoncpp_CFLAGS) \
	$(libtinyxml2_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS) \
	$(FILESYSTEM_LIBS)

libcloudstorage_ladir = $(includedir)/cloudstorage
lib_LTLIBRARIES = libcloudstorage.la

libcloudstorage_la_SOURCES = \
	Utility/CloudStorage.cpp \
	Utility/Auth.cpp \
	Utility/Item.cpp \
	Utility/Utility.cpp \
	Utility/CryptoPP.cpp \
	Utility/CurlHttp.cpp \
	Utility/MicroHttpdServer.cpp \
	Utility/ThreadPool.cpp \
	Utility/FileServer.cpp \
	CloudProvider/CloudProvider.cpp \
	CloudProvider/GoogleDrive.cpp \
	CloudProvider/OneDrive.cpp \
	CloudProvider/Dropbox.cpp \
	CloudProvider/AmazonS3.cpp \
	CloudProvider/Box.cpp \
	CloudProvider/YouTube.cpp \
	CloudProvider/YandexDisk.cpp \
	CloudProvider/WebDav.cpp \
	CloudProvider/PCloud.cpp \
	CloudProvider/HubiC.cpp \
	CloudProvider/GooglePhotos.cpp \
	CloudProvider/MegaNz.cpp \
	CloudProvider/LocalDrive.cpp \
	CloudProvider/LocalDriveWinRT.cpp \
	CloudProvider/AnimeZone.cpp \
	CloudProvider/FourShared.cpp \
	Request/Request.cpp \
	Request/HttpCallback.cpp \
	Request/AuthorizeRequest.cpp \
	Request/DownloadFileRequest.cpp \
	Request/GetItemRequest.cpp \
	Request/ListDirectoryRequest.cpp \
	Request/ListDirectoryPageRequest.cpp \
	Request/UploadFileRequest.cpp \
	Request/GetItemDataRequest.cpp \
	Request/DeleteItemRequest.cpp \
	Request/CreateDirectoryRequest.cpp \
	Request/MoveItemRequest.cpp \
	Request/RenameItemRequest.cpp \
	Request/ExchangeCodeRequest.cpp \
	Request/GetItemUrlRequest.cpp \
	Request/RecursiveRequest.cpp \
	C/CloudProvider.cpp \
	C/CloudStorage.cpp \
	C/Crypto.cpp \
	C/Http.cpp \
	C/HttpServer.cpp \
	C/Item.cpp \
	C/Request.cpp \
	C/ThreadPool.cpp

noinst_HEADERS = \
	IAuth.h \
	Utility/CloudStorage.h \
	Utility/Auth.h \
	Utility/Item.h \
	Utility/Utility.h \
	Utility/CryptoPP.h \
	Utility/CurlHttp.h \
	Utility/MicroHttpdServer.h \
	Utility/ThreadPool.h \
	Utility/FileServer.h \
	Utility/JQuery.h \
	Utility/UrlJS.h \
	CloudProvider/CloudProvider.h \
	CloudProvider/GoogleDrive.h \
	CloudProvider/OneDrive.h \
	CloudProvider/Dropbox.h \
	CloudProvider/Box.h \
	CloudProvider/AmazonS3.h \
	CloudProvider/YouTube.h \
	CloudProvider/YandexDisk.h \
	CloudProvider/WebDav.h \
	CloudProvider/PCloud.h \
	CloudProvider/HubiC.h \
	CloudProvider/GooglePhotos.h \
	CloudProvider/MegaNz.h \
	CloudProvider/LocalDrive.h \
	CloudProvider/LocalDriveWinRT.h \
	CloudProvider/AnimeZone.h \
	CloudProvider/FourShared.h \
	Request/HttpCallback.h \
	Request/AuthorizeRequest.h \
	Request/Request.h \
	Request/DownloadFileRequest.h \
	Request/GetItemRequest.h \
	Request/GetItemDataRequest.h \
	Request/ListDirectoryRequest.h \
	Request/ListDirectoryPageRequest.h \
	Request/UploadFileRequest.h \
	Request/DeleteItemRequest.h \
	Request/CreateDirectoryRequest.h \
	Request/MoveItemRequest.h \
	Request/RenameItemRequest.h \
	Request/ExchangeCodeRequest.h \
	Request/GetItemUrlRequest.h \
	Request/RecursiveRequest.h

libcloudstorage_la_HEADERS = \
	IItem.h \
	ICloudProvider.h \
	ICloudStorage.h \
	IRequest.h \
	ICrypto.h \
	IHttp.h \
	IHttpServer.h \
	IThreadPool.h

libcloudstorage_cinterfacedir=$(libcloudstorage_ladir)/C
libcloudstorage_cinterface_HEADERS = \
	C/CloudProvider.h \
	C/CloudStorage.h \
	C/Crypto.h \
	C/Http.h \
	C/HttpServer.h \
	C/Item.h \
	C/Request.h \
	C/ThreadPool.h

libcloudstorage_la_LIBADD = \
	$(libjsoncpp_LIBS) \
	$(libtinyxml2_LIBS)

if WITH_MEGA
AM_CXXFLAGS += $(libmega_CFLAGS)
libcloudstorage_la_LIBADD += $(libmega_LIBS)
endif

if WITH_CRYPTOPP
AM_CXXFLAGS += $(libcryptopp_CFLAGS)
libcloudstorage_la_LIBADD += $(libcryptopp_LIBS)
endif

if WITH_CURL
AM_CXXFLAGS += $(libcurl_CFLAGS)
libcloudstorage_la_LIBADD += $(libcurl_LIBS)
endif

if WITH_MICROHTTPD
AM_CXXFLAGS += $(libmicrohttpd_CFLAGS)
libcloudstorage_la_LIBADD += $(libmicrohttpd_LIBS)
endif

if ANDROID
libcloudstorage_la_LIBADD += -llog
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcloudstorage.pc
