ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libcurl_CFLAGS) \
	$(libjsoncpp_CFLAGS) \
	$(libmicrohttpd_CFLAGS)

AM_LDFLAGS = \
	-no-undefined \
	$(SOCKET_LIBS)

lib_LTLIBRARIES = libcloudstorage.la

libcloudstorage_la_SOURCES = \
	Utility/CloudStorage.cpp \
	Utility/Auth.cpp \
	Utility/Item.cpp \
	Utility/HttpRequest.cpp \
	CloudProvider/CloudProvider.cpp \
	CloudProvider/GoogleDrive.cpp \
	CloudProvider/OneDrive.cpp \
	CloudProvider/Dropbox.cpp \
	CloudProvider/AmazonDrive.cpp \
	CloudProvider/Box.cpp \
	CloudProvider/YouTube.cpp \
	CloudProvider/YandexDisk.cpp \
	Request/Request.cpp \
	Request/HttpCallback.cpp \
	Request/AuthorizeRequest.cpp \
	Request/DownloadFileRequest.cpp \
	Request/GetItemRequest.cpp \
	Request/ListDirectoryRequest.cpp \
	Request/UploadFileRequest.cpp \
	Request/GetItemDataRequest.cpp \
	Request/DeleteItemRequest.cpp \
	Request/CreateDirectoryRequest.cpp \
	Request/MoveItemRequest.cpp

noinst_HEADERS = \
	IAuth.h \
	Utility/CloudStorage.h \
	Utility/Auth.h \
	Utility/Item.h \
	Utility/Utility.h \
	Utility/HttpRequest.h \
	CloudProvider/CloudProvider.h \
	CloudProvider/GoogleDrive.h \
	CloudProvider/OneDrive.h \
	CloudProvider/Dropbox.h \
	CloudProvider/Box.h \
	CloudProvider/AmazonDrive.h \
	CloudProvider/YouTube.h \
	CloudProvider/YandexDisk.h \
	Request/HttpCallback.h \
	Request/AuthorizeRequest.h \
	Request/Request.h \
	Request/DownloadFileRequest.h \
	Request/GetItemRequest.h \
	Request/ListDirectoryRequest.h \
	Request/UploadFileRequest.h \
	Request/DeleteItemRequest.h \
	Request/CreateDirectoryRequest.h \
	Request/MoveItemRequest.h

nobase_include_HEADERS = \
	IItem.h \
	ICloudProvider.h \
	ICloudStorage.h \
	IRequest.h

libcloudstorage_la_LIBADD = \
	$(libcurl_LIBS) \
	$(libjsoncpp_LIBS) \
	$(libmicrohttpd_LIBS)

if WITH_MEGA
AM_CXXFLAGS += $(libmega_CFLAGS)
libcloudstorage_la_LIBADD += $(libmega_LIBS)
libcloudstorage_la_SOURCES += CloudProvider/MegaNz.cpp
noinst_HEADERS += CloudProvider/MegaNz.h
endif
