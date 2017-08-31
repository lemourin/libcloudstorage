ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libjsoncpp_CFLAGS) \
	$(libtinyxml2_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

libcloudstorage_ladir = $(includedir)/cloudstorage
lib_LTLIBRARIES = libcloudstorage.la

libcloudstorage_la_SOURCES = \
	Utility/CloudStorage.cpp \
	Utility/Auth.cpp \
	Utility/Item.cpp \
	Utility/Utility.cpp \
	CloudProvider/CloudProvider.cpp \
	CloudProvider/GoogleDrive.cpp \
	CloudProvider/OneDrive.cpp \
	CloudProvider/Dropbox.cpp \
	CloudProvider/AmazonDrive.cpp \
	CloudProvider/AmazonS3.cpp \
	CloudProvider/Box.cpp \
	CloudProvider/YouTube.cpp \
	CloudProvider/YandexDisk.cpp \
	CloudProvider/WebDav.cpp \
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
	Request/ExchangeCodeRequest.cpp

noinst_HEADERS = \
	IAuth.h \
	Utility/CloudStorage.h \
	Utility/Auth.h \
	Utility/Item.h \
	Utility/Utility.h \
	CloudProvider/CloudProvider.h \
	CloudProvider/GoogleDrive.h \
	CloudProvider/OneDrive.h \
	CloudProvider/Dropbox.h \
	CloudProvider/Box.h \
	CloudProvider/AmazonDrive.h \
	CloudProvider/AmazonS3.h \
	CloudProvider/YouTube.h \
	CloudProvider/YandexDisk.h \
	CloudProvider/WebDav.h \
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
	Request/ExchangeCodeRequest.h

libcloudstorage_la_HEADERS = \
	IItem.h \
	ICloudProvider.h \
	ICloudStorage.h \
	IRequest.h \
	ICrypto.h \
	IHttp.h \
	IHttpServer.h

libcloudstorage_la_LIBADD = \
	$(libjsoncpp_LIBS) \
	$(libtinyxml2_LIBS)

if WITH_MEGA
AM_CXXFLAGS += $(libmega_CFLAGS)
libcloudstorage_la_LIBADD += $(libmega_LIBS)
libcloudstorage_la_SOURCES += CloudProvider/MegaNz.cpp
noinst_HEADERS += CloudProvider/MegaNz.h
endif

if WITH_CRYPTOPP
AM_CXXFLAGS += $(libcryptopp_CFLAGS)
libcloudstorage_la_LIBADD += $(libcryptopp_LIBS)
libcloudstorage_la_SOURCES += Utility/CryptoPP.cpp
noinst_HEADERS += Utility/CryptoPP.h
endif

if WITH_CURL
AM_CXXFLAGS += $(libcurl_CFLAGS)
libcloudstorage_la_LIBADD += $(libcurl_LIBS)
libcloudstorage_la_SOURCES += Utility/CurlHttp.cpp
noinst_HEADERS += Utility/CurlHttp.h
endif

if WITH_MICROHTTPD
AM_CXXFLAGS += $(libmicrohttpd_CFLAGS)
libcloudstorage_la_LIBADD += $(libmicrohttpd_LIBS)
libcloudstorage_la_SOURCES += Utility/MicroHttpdServer.cpp
noinst_HEADERS += Utility/MicroHttpdServer.h
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcloudstorage.pc
