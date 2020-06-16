include(CheckIncludeFiles)

add_library(cloudstorage-c-api OBJECT)

set(cloudstorage-c-api_PUBLIC_HEADERS
    C/CloudProvider.h
    C/CloudStorage.h
    C/Crypto.h
    C/Http.h
    C/HttpServer.h
    C/Item.h
    C/Request.h
    C/ThreadPool.h
)

target_sources(cloudstorage-c-api PRIVATE
    C/CloudProvider.cpp
    C/CloudStorage.cpp
    C/Crypto.cpp
    C/Http.cpp
    C/HttpServer.cpp
    C/Item.cpp
    C/Request.cpp
    C/ThreadPool.cpp
    ${cloudstorage-c-api_PUBLIC_HEADERS}
)

add_library(cloudstorage)

set(cloudstorage_PUBLIC_HEADERS
    IAuth.h
    ICloudAccess.h
    ICloudFactory.h
    ICloudProvider.h
    ICloudStorage.h
    ICrypto.h
    IHttp.h
    IHttpServer.h
    IItem.h
    IRequest.h
    IThreadPool.h
)

target_sources(cloudstorage PRIVATE
    CloudProvider/AmazonS3.cpp
    CloudProvider/AmazonS3.h
    CloudProvider/AnimeZone.cpp
    CloudProvider/AnimeZone.h
    CloudProvider/Box.cpp
    CloudProvider/Box.h
    CloudProvider/CloudProvider.cpp
    CloudProvider/CloudProvider.h
    CloudProvider/Dropbox.cpp
    CloudProvider/Dropbox.h
    CloudProvider/FourShared.cpp
    CloudProvider/FourShared.h
    CloudProvider/GoogleDrive.cpp
    CloudProvider/GoogleDrive.h
    CloudProvider/GooglePhotos.cpp
    CloudProvider/GooglePhotos.h
    CloudProvider/HubiC.cpp
    CloudProvider/HubiC.h
    CloudProvider/LocalDrive.cpp
    CloudProvider/LocalDrive.h
    CloudProvider/LocalDriveWinRT.cpp
    CloudProvider/LocalDriveWinRT.h
    CloudProvider/MegaNz.cpp
    CloudProvider/MegaNz.h
    CloudProvider/OneDrive.cpp
    CloudProvider/OneDrive.h
    CloudProvider/PCloud.cpp
    CloudProvider/PCloud.h
    CloudProvider/WebDav.cpp
    CloudProvider/WebDav.h
    CloudProvider/YandexDisk.cpp
    CloudProvider/YandexDisk.h
    Request/AuthorizeRequest.cpp
    Request/AuthorizeRequest.h
    Request/CreateDirectoryRequest.cpp
    Request/CreateDirectoryRequest.h
    Request/DeleteItemRequest.cpp
    Request/DeleteItemRequest.h
    Request/DownloadFileRequest.cpp
    Request/DownloadFileRequest.h
    Request/ExchangeCodeRequest.cpp
    Request/ExchangeCodeRequest.h
    Request/GetItemDataRequest.cpp
    Request/GetItemDataRequest.h
    Request/GetItemRequest.cpp
    Request/GetItemRequest.h
    Request/GetItemUrlRequest.cpp
    Request/GetItemUrlRequest.h
    Request/HttpCallback.cpp
    Request/HttpCallback.h
    Request/ListDirectoryPageRequest.cpp
    Request/ListDirectoryPageRequest.h
    Request/ListDirectoryRequest.cpp
    Request/ListDirectoryRequest.h
    Request/MoveItemRequest.cpp
    Request/MoveItemRequest.h
    Request/RecursiveRequest.cpp
    Request/RecursiveRequest.h
    Request/RenameItemRequest.cpp
    Request/RenameItemRequest.h
    Request/Request.cpp
    Request/Request.h
    Request/UploadFileRequest.cpp
    Request/UploadFileRequest.h
    Utility/Auth.cpp
    Utility/Auth.h
    Utility/CloudAccess.cpp
    Utility/CloudAccess.h
    Utility/CloudEventLoop.cpp
    Utility/CloudEventLoop.h
    Utility/CloudFactory.cpp
    Utility/CloudFactory.h
    Utility/CloudStorage.cpp
    Utility/CloudStorage.h
    Utility/CryptoPP.cpp
    Utility/CryptoPP.h
    Utility/CurlHttp.cpp
    Utility/CurlHttp.h
    Utility/FileServer.cpp
    Utility/FileServer.h
    Utility/GenerateThumbnail.cpp
    Utility/GenerateThumbnail.h
    Utility/HttpServer.cpp
    Utility/HttpServer.h
    Utility/Item.cpp
    Utility/Item.h
    ${CMAKE_CURRENT_BINARY_DIR}/LoginPage.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/LoginPage.h
    Utility/MicroHttpdServer.cpp
    Utility/MicroHttpdServer.h
    Utility/Promise.h
    Utility/ThreadPool.cpp
    Utility/ThreadPool.h
    Utility/Utility.cpp
    Utility/Utility.h
    ${cloudstorage_PUBLIC_HEADERS}
)

set_target_properties(cloudstorage 
    PROPERTIES 
        PUBLIC_HEADER "${cloudstorage_PUBLIC_HEADERS}"
        CXX_STANDARD 17
)
set_target_properties(cloudstorage-c-api
    PROPERTIES
        PUBLIC_HEADER "${cloudstorage-c-api_PUBLIC_HEADERS}"
        POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
        CXX_STANDARD 17
)

set(cloudstorage-resources
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/4shared_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/amazons3_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/animezone_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/local_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/localwinrt_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/mega_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/webdav_login.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/default_error.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/default_success.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/bootstrap.min.css
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/bootstrap.min.js
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/url.min.js
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/style.min.css
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/jquery.min.js
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/cloud.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/cloudbrowser/resources/vlc-blue.png
)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/LoginPage.cpp ${CMAKE_CURRENT_BINARY_DIR}/LoginPage.h
    COMMAND resource-generator ${CMAKE_CURRENT_BINARY_DIR}/LoginPage ${cloudstorage-resources}
    DEPENDS ${cloudstorage-resources} resource-generator
)

target_link_libraries(cloudstorage PRIVATE Threads::Threads cloudstorage-c-api)

cloudstorage_target_link_library(cloudstorage jsoncpp)
cloudstorage_target_link_library(cloudstorage-c-api jsoncpp)

cloudstorage_find_library(tinyxml2 INCLUDE tinyxml2.h NAMES tinyxml2 DEBUG_NAMES tinyxml2d)
cloudstorage_target_link_library(cloudstorage tinyxml2)

if(curl_LIBRARY)
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(cloudstorage PRIVATE CURL_STATICLIB)
    endif()
    target_compile_definitions(cloudstorage PRIVATE WITH_CURL)
    cloudstorage_target_link_library(cloudstorage curl)
endif()

if(microhttpd_LIBRARY)
    target_compile_definitions(cloudstorage PRIVATE WITH_MICROHTTPD)
    cloudstorage_target_link_library(cloudstorage microhttpd)
endif()

if(ffmpeg_FOUND)
    target_compile_definitions(cloudstorage PRIVATE WITH_THUMBNAILER)
    cloudstorage_target_link_library(cloudstorage ffmpeg)
endif()

find_package(Boost COMPONENTS filesystem)
if(Boost_FILESYSTEM_FOUND)
    target_compile_definitions(cloudstorage PRIVATE HAVE_BOOST_FILESYSTEM_HPP)
    target_link_libraries(cloudstorage PRIVATE Boost::filesystem)
endif()

cloudstorage_find_library(mega INCLUDE mega.h NAMES mega)
if(mega_LIBRARY)
    target_compile_definitions(cloudstorage PRIVATE WITH_MEGA)
    cloudstorage_target_link_library(cloudstorage mega)
    if(WIN32)
        if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
            target_compile_definitions(cloudstorage PRIVATE WINDOWS_PHONE SQLITE_OS_WINRT)
        endif()
        target_compile_definitions(cloudstorage PRIVATE WIN32_LEAN_AND_MEAN ZLIB_WINAPI USE_CRYPTOPP USE_CURL USE_OPENSSL USE_SQLITE ENABLE_SYNC)
        target_include_directories(cloudstorage PRIVATE ${mega_INCLUDE_DIR}/mega/win32)
    else()
        target_include_directories(cloudstorage PRIVATE ${mega_INCLUDE_DIR}/mega/posix)
    endif()
endif()

cloudstorage_find_library(cryptopp INCLUDE cryptopp NAMES cryptopp cryptopp-static cryptlib)
if(cryptopp_LIBRARY)
    target_compile_definitions(cloudstorage PRIVATE WITH_CRYPTOPP)
    cloudstorage_target_link_library(cloudstorage cryptopp)
endif()

target_include_directories(cloudstorage PUBLIC "." ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(cloudstorage-c-api PUBLIC ".")

check_include_files(jni.h HAVE_JNI_H)
if(HAVE_JNI_H)
    target_compile_definitions(cloudstorage PRIVATE HAVE_JNI_H)
endif()

target_compile_definitions(cloudstorage PRIVATE
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    _CRT_SECURE_NO_WARNINGS
)

target_compile_definitions(cloudstorage PUBLIC UNICODE)
target_compile_definitions(cloudstorage-c-api PUBLIC UNICODE)

if(MSVC)
    target_compile_options(cloudstorage PRIVATE /wd4996 /wd4251 /wd4275 /Zc:__cplusplus /EHsc)
    target_compile_options(cloudstorage-c-api PRIVATE /wd4996 /wd4251 /wd4275 /Zc:__cplusplus /EHsc)
endif()

if(WIN32)
    target_link_libraries(cloudstorage PRIVATE ws2_32)
endif()

if(ANDROID)
    target_link_libraries(cloudstorage PUBLIC log)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" AND MSVC)
    target_compile_options(cloudstorage PRIVATE /await)
    target_link_options(cloudstorage BEFORE PRIVATE
        $<IF:$<CONFIG:Debug>,/nodefaultlib:vccorlibd /nodefaultlib:msvcrtd vccorlibd.lib msvcrtd.lib,/nodefaultlib:vccorlib /nodefaultlib:msvcrt vccorlib.lib msvcrt.lib>
    )
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(cloudstorage PRIVATE CLOUDSTORAGE_LIBRARY)
    target_compile_definitions(cloudstorage-c-api PRIVATE CLOUDSTORAGE_LIBRARY)
else()
    target_compile_definitions(cloudstorage PUBLIC CLOUDSTORAGE_STATIC_LINK)
    target_compile_definitions(cloudstorage-c-api PUBLIC CLOUDSTORAGE_STATIC_LINK)
endif()

install(TARGETS cloudstorage PUBLIC_HEADER DESTINATION include/cloudstorage)
install(TARGETS cloudstorage-c-api PUBLIC_HEADER DESTINATION include/cloudstorage/C)
