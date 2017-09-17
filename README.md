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

Optional dependency notes:

* `libcryptopp`:

  when  not  found,  `ICrypto`  interface  needs to  be  implemented,  can  be  
  explicitly disabled with `--with-cryptopp=no`

* `libcurl`

  when not found, `IHttp` interface needs to be implemented, can be explicitly  
  disabled with `--with-curl=no`

* `libmicrohttpd`

  when  not found,  `IHttpServer` interface  needs to  be implemented,  can be  
  explicitly disabled with `--with-microhttpd=no`

* `mega`

  when  not  found,  `mega`  cloud  provider will  not  be  included,  can  be  
  explicitly disabled with `--with-mega=no`

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

FUSE:
=====

In `examples/fuse` there is implemented a user space file system using `fuse3`  
(https://github.com/libfuse/libfuse) library. It will be build when `fuse3` is  
found  and  `--with-examples`  configure  flag  is  passed.  The  file  system  
is  implemented  using `libfuse`'s  low  level  api;  however high  level  api  
implementation is  also provided. The  file system supports  moving, renaming,  
creating  directories, reading  and writing  new files.  Writing over  already  
present  files in  cloud  provider  is not  supported.  The  file system  uses  
asynchronous  I/O to  its full  potency. It  doesn't cache  files anywhere  by  
itself which implies no local storage  overhead. Most cloud providers are fast  
enough when it comes to watching  videos; with `mega.nz` being the fastest and  
`Google Drive` being the slowest.

## Usage:

To add cloud providers to file system, first the cloud providers need to be
added. This can be done by calling:

`libcloudstorage-fuse --add=provider_label`

After cloud providers are added, the file system can be mount using:

`libcloudstorage-fuse mountpoint`

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
* `Dokan fuse`

Implement chunked uploads.
