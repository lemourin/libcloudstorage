# libcloudstorage

A `C++`  library providing access to  files located in various  cloud services  
licensed under `GNU LGPLv2.1`. It is  focused on the basic operations on those  
services.

Supported cloud providers:
==========================

* `GoogleDrive`
* `OneDrive`
* `Dropbox`
* `AmazonDrive`
* `box.com`
* `YandexDisk`
* `WebDAV`
* `mega.nz`
* `AmazonS3`
* `pCloud`
* `YouTube` (partial)

Supported operations on files:
==============================

* `list directory`
* `download file`
* `upload file`
* `get thumbnail`
* `delete file`
* `create directory`
* `move file`
* `rename file`
* `fetch direct, preauthenticated url to file`

Requirements:
=============

* [`jsoncpp`](https://github.com/open-source-parsers/jsoncpp)
* [`tinyxml2`](https://github.com/leethomason/tinyxml2)
* [`libmicrohttpd`](https://www.gnu.org/software/libmicrohttpd/) (optional)
* [`cURL`](https://curl.haxx.se/) (with `OpenSSL`/`c-ares`, optional)
* [`libcryptopp`](https://www.cryptopp.com/) (optional, required for `AmazonS3`)
* [`mega`](https://github.com/meganz/sdk) (optional, required for `mega.nz`)

Platforms:
==========

The library can be  build on any platform which has  a proper `C++11` support.  
It was tested under the following operating systems:

* `Ubuntu 16.04`
* `Windows 10`
* `Android 5.0.2`

Building:
=========

The generic way to build and install it is:

* `./bootstrap`
* `./configure`
* `make`
* `sudo make install`

`./configure` flags:

* `--with-cryptopp`

   library  will  use  `cryptopp`'s   hashing  functions;  without  this  flag  
   user  will  have  to  implement   `ICrypto`'s  interface  and  pass  it  to  
   `ICloudProvider`'s initialize method

* `--with-curl`

   library will use `curl` to make all `http` requests; without this user will  
   have to implement `IHttp` interface

* `--with-microhttpd`

   library  will use  `libmicrohttpd`  as  a http  server,  it's required  for  
   retrieving the oauth  authorization code; without this flag  user will have  
   to implement `IHttpServer` interface

* `--with-mega=no`

   if  you  don't  have  access  to  `Mega  SDK`,  pass  this  flag  to  build  
   `libcloudstorage` without it

Cloud Browser:
==============

Optionally, you can build an example program which provides easy graphics user  
interface for all the features implemented in `libcloudstorage`. Related cloud  
browser `./configure` options are:

* `--with-examples`

   enables  the  build of  cloud  browser  and  few other  examples;  requires  
   `Qt5Core`,  `Qt5Gui`,  `Qt5Quick`;  when `libffmpegthumbnailer`  is  found,  
   cloud browser will  generate fallback thumbnails if  cloud provider doesn't  
   provide any

* `--with-vlc`

   cloud  browser will  use `vlc`  to play  media files;  requires `libvlcpp`,  
   `libvlc`, `Qt5Widgets`

* `--with-qtmultimediawidgets`

   cloud browser  will use  `Qt5MultimediaWidget` based  player to  play media  
   files; requires `Qt5Widgets`, `Qt5Multimedia`, `Qt5MultimediaWidgets`

* `--with-qmlplayer`

   cloud  browser   will  use  `qml`   based  player  to  play   media  files;  
   requires  `Qt5Multimedia`; if  user  doesn't specify  any of  `--with-vlc`,  
   `--with-qtmultimediawidgets`, `qml` player will be used

Screenshot:

  <a href="https://i.imgur.com/yqiydaD.png">
    <img src="https://i.imgur.com/yqiydaD.png" width="480" height="270" />
  </a>

TODO:
=====

Implement following cloud providers:
* `Apple ICloud`
* `Asus WebStorage`
* `Baidu Cloud`
* `CloudMe`
* `FileDropper`
* `Fileserve`
* `Handy Backup`
* `IBM Connections`
* `Jumpshare`
* `MagicVortex`
* `MediaFire`
* `Pogoplug`
* `SpiderOak`
* `SugarSync`
* `Tencent Weiyun`
* `TitanFile`
* `Tresorit`
* `XXL Box`

Implement bindings to various languages, notably script languages:
* `Obj-C`
* `python`
* `ruby`
* `JavaScript` / `node`
* `Java`

Integrate in various desktops
* `KIO` slave
* `gvfs` implementation
* `fuse`
* `Dokan fuse`
