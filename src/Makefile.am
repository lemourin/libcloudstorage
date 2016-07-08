ACLOCAL_AMFLAGS = -I m4

AM_CXXFLAGS = \
	$(libcurl_CXXFLAGS) \
	$(libjsoncpp_CXXFLAGS) \
	$(libmicrohttpd_CXXFLAGS)

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
	Request/Request.cpp \
	Request/HttpCallback.cpp \
	Request/AuthorizeRequest.cpp \
	Request/DownloadFileRequest.cpp \
	Request/GetItemRequest.cpp \
	Request/ListDirectoryRequest.cpp \
	Request/UploadFileRequest.cpp \
	Request/GetItemDataRequest.cpp

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
	CloudProvider/AmazonDrive.h \
	Request/HttpCallback.h \
	Request/AuthorizeRequest.h

nobase_include_HEADERS = \
	IItem.h \
	ICloudProvider.h \
	ICloudStorage.h \
	Request/Request.h \
	Request/DownloadFileRequest.h \
	Request/GetItemRequest.h \
	Request/ListDirectoryRequest.h \
	Request/UploadFileRequest.h

libcloudstorage_la_LIBADD = \
	$(libcurl_LIBS) \
	$(libjsoncpp_LIBS) \
	$(libmicrohttpd_LIBS)
