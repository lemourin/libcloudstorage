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
	Auth.cpp \
	Item.cpp \
	HttpRequest.cpp \
	CloudStorage.cpp \
	CloudProvider/CloudProvider.cpp \
	CloudProvider/GoogleDrive.cpp \
	CloudProvider/OneDrive.cpp \
	CloudProvider/Dropbox.cpp \
	Request/Request.cpp \
	Request/HttpCallback.cpp \
	Request/AuthorizeRequest.cpp \
	Request/DownloadFileRequest.cpp \
	Request/GetItemRequest.cpp \
	Request/ListDirectoryRequest.cpp \
	Request/UploadFileRequest.cpp

noinst_HEADERS = \
	Utility.h \
	IAuth.h \
	Auth.h \
	Item.h \
	HttpRequest.h \
	CloudProvider/CloudProvider.h \
	CloudProvider/GoogleDrive.h \
	CloudProvider/OneDrive.h \
	CloudProvider/Dropbox.h \
	Request/HttpCallback.h

nobase_include_HEADERS = \
	IItem.h \
	ICloudProvider.h \
	ICloudStorage.h \
	CloudStorage.h \
	Request/Request.h \
	Request/AuthorizeRequest.h \
	Request/DownloadFileRequest.h \
	Request/GetItemRequest.h \
	Request/ListDirectoryRequest.h \
	Request/UploadFileRequest.h

libcloudstorage_la_LIBADD = \
	$(libcurl_LIBS) \
	$(libjsoncpp_LIBS) \
	$(libmicrohttpd_LIBS)
