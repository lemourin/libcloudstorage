# libcloudstorage

C++ library providing access to files located in cloud services
licensed under GNU LGPLv2.1.

Supported cloud providers:
==========================

* GoogleDrive
* OneDrive
* Dropbox
* AmazonDrive
* box.com
* YandexDisk
* mega.nz
* AmazonS3
* YouTube (partial)

Supported operations on files:
==============================

* list directory
* download file
* upload file
* get thumbnail
* delete file
* create directory
* move file
* rename file
* fetch direct, preauthenticated url to file

Requirements:
=============

* libmicrohttpd
* jsoncpp
* tinyxml2
* cURL (with OpenSSL/c-ares, optional)
* libcryptopp (optional)
* mega (optional, required for mega.nz)

Platforms:
==========

The library can be build on any platform which has a proper C++11 support. It
was tested under the following operating systems:

* Ubuntu 16.04
* Windows 10
* Android 5.0.2

Building:
===============

The generic way to build and install it is:

* ./bootstrap
* ./configure
* make
* sudo make install

Optional flags: 
* --with-cryptopp: library will use cryptopp's hashing functions, without this
flag user will have to implement ICrypto's interface and pass it to 
ICloudProvider's initialize method
* --with-curl: library will use curl to make all http requests, without this
user will have to implement IHttp interface
* --with-thumbnailer: library will use ffmpegthumbnailer to generate thumbnails
for files which don't have them provided by the cloud storage service, without
this flag user may provide his own thumbnailer engine by implementing 
IThumbnailer interface
* --with-mega=no: if you don't have access to Mega SDK, pass this flag to build
libcloudstorage without it

Cloud Browser:
=============

Optionally, you can build an example program which provides easy graphics user
interface for all the features implemented in libcloudstorage. You just have to
pass --with-examples to ./configure. Cloud browser also allows playing streams 
from cloud providers, by default it uses qml player as a multimedia player 
(--with-qmlplayer). You can choose libvlc as a player with --with-vlc or
Qt5MultimediaWidgets based player with --with-qtmultimediawidgets.

It requires:
* Qt5Core
* Qt5Gui
* Qt5Quick
* Qt5Multimedia if --with-qmlplayer (default)
* Qt5Widgets, Qt5Multimedia, Qt5MultimediaWidgets if --with-qtmultimediawidgets
* libvlcpp, libvlc, Qt5Widgets if --with-vlc
* libcloudstorage built with at least cURL

TODO:
=====

Implement following cloud providers:
* Asus WebStorage
* Baidu Cloud
* CloudMe
* FileDropper
* Fileserve
* Handy Backup
* IBM Connections
* Apple ICloud
* Infinit
* Jumpshare
* MagicVortex
* MediaFire
* Pogoplug
* SpiderOak
* SugarSync
* Tencent Weiyun
* TitanFile
* Tresorit
* XXL Box
