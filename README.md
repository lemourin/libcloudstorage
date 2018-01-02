# libcloudstorage
[![Build Status](https://travis-ci.org/lemourin/libcloudstorage.svg?branch=master)](https://travis-ci.org/lemourin/libcloudstorage)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/14018/badge.svg)](https://scan.coverity.com/projects/lemourin-libcloudstorage)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/927fdff765294da3964e92194193d2b4)](https://www.codacy.com/app/lemourin/libcloudstorage?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=lemourin/libcloudstorage&amp;utm_campaign=Badge_Grade)
[![Discord](https://img.shields.io/badge/chat-discord-brightgreen.svg)](https://discord.gg/BJajAKZ)
[![License: LGPL v2.1](https://img.shields.io/badge/license-LGPL%20v2.1-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1.html)

A `C++`  library providing access to  files located in various  cloud services  
licensed under `GNU LGPLv2.1`. It is  focused on the basic operations on those  
services.

Supported cloud providers:
==========================

* `GoogleDrive`
* `OneDrive`
* `Dropbox`
* `box.com`
* `YandexDisk`
* `WebDAV`
* `mega.nz`
* `AmazonS3`
* `pCloud`
* `hubiC`
* `Google Photos` (partial)
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

* `boost-filesystem`

  when  found,  `LocalDrive` provider  representing  local  directory will  be  
  included, can be explicitly disabled with `--with-filesystem=no`

* `mega`

  when  not  found,  `mega`  cloud  provider will  not  be  included,  can  be  
  explicitly disabled with `--with-mega=no`

FUSE:
=====

In `bin/fuse`  there is  implemented a  user space  file  system  using `fuse`  
(https://github.com/libfuse/libfuse) library. It will be  build when `fuse` is  
found  (unless explicitly  disabled  with `--with-fuse=no`).  The file  system  
is  implemented  using `libfuse`'s  low  level  api;  however high  level  api  
implementation is  also provided. The  file system supports  moving, renaming,  
creating  directories, reading  and writing  new files.  Writing over  already  
present  files in  cloud  provider  is not  supported.  The  file system  uses  
asynchronous  I/O to  its full  potency. It  doesn't cache  files anywhere  by  
itself which implies no local storage  overhead. Most cloud providers are fast  
enough when it comes to watching  videos; with `mega.nz` being the fastest and  
`Google Drive` being the slowest.

## Windows:

It is possible  to run `libcloudstorage-fuse` under  `Windows` using `Dokan`'s  
(https://github.com/dokan-dev/dokany) `fuse` wrapper.

## Usage:

To add cloud providers to file system, first the cloud providers need to be  
added. This can be done by calling:

`libcloudstorage-fuse --add=provider_label`

After cloud providers are added, the file system can be mount using:

`libcloudstorage-fuse mountpoint`

Cloud Browser:
==============

In `bin/cloudbrowser`  there is  a program which  provides easy  graphics user  
interface for  all the features  implemented in `libcloudstorage`. It  will be  
built  when  its  dependencies  are found,  unless  explicitly  disabled  with  
`--with-cloudbrowser=no`.

Cloud Browser dependencies:

* `Qt5Core`, `Qt5Gui`, `Qt5Quick`

* `kirigami` (https://github.com/KDE/kirigami)

* `Qt5WebView`

  when found, Cloud Browser will use it to present the authorization scheme

* `libffmpegthumbnailer` 

  when  found,  Cloud  Browser  will generate  fallback  thumbnails  if  cloud  
  provider doesn't provide any

* `vlc-qt` (https://github.com/vlc-qt/vlc-qt)

  when  found, Cloud  Browser  will  use  vlc-based  media  player instead  of  
  QtMultimedia-based one

Screenshot:

  <a href="https://i.imgur.com/NsbNvfX.png">
    <img src="https://i.imgur.com/NsbNvfX.png" width="640" />
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
* `Dokan fuse`

Implement chunked uploads.
