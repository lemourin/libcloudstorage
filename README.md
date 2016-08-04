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

* cURL (with OpenSSL/c-ares)
* libmicrohttpd
* jsoncpp
* tinyxml2
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

Optionally, you can pass --with-cryptopp flag to ./configure; library then will
use cryptopp's hashing functions, otherwise, user will have to provide
ICrypto's interface implementation.

If you don't have access to Mega SDK, use --with-mega=no to build the library
without it.

Cloud browser:
=============

Optionally, you can build an example program which provides easy graphics user
interface for all the features implemented in libcloudstorage. You just have to
pass --with-examples flag to ./configure. Cloud browser also allows playing
streams from cloud providers, by default it uses qml player as a multimedia
player (--with-qmlplayer). You can choose libvlc as a player with --with-vlc or
Qt5MultimediaWidgets based player with --with-qtmultimediawidgets.

It requires:
* Qt5Core
* Qt5Gui
* Qt5Quick
* Qt5Multimedia if --with-qmlplayer (default)
* Qt5Widgets, Qt5Multimedia, Qt5MultimediaWidgets if --with-qtmultimediawidgets
* libvlcpp, libvlc, Qt5Widgets if --with-vlc
