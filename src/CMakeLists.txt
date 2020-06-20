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

add_library(cloudstorage-util OBJECT)

set(cloudstorage-util_PUBLIC_HEADERS
    Utility/Promise.h
)

target_sources(cloudstorage-util PRIVATE
    Utility/MicroHttpdServer.cpp
    Utility/MicroHttpdServer.h
    Utility/Promise.h
    Utility/ThreadPool.cpp
    Utility/ThreadPool.h
    Utility/Utility.cpp
    Utility/Utility.h
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
    ${cloudstorage-util_PUBLIC_HEADERS}
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

set(cloudstorage_HEADERS
    CloudProvider/AmazonS3.h
    CloudProvider/AnimeZone.h
    CloudProvider/Box.h
    CloudProvider/CloudProvider.h
    CloudProvider/Dropbox.h
    CloudProvider/FourShared.h
    CloudProvider/GoogleDrive.h
    CloudProvider/GooglePhotos.h
    CloudProvider/HubiC.h
    CloudProvider/LocalDrive.h
    CloudProvider/LocalDriveWinRT.h
    CloudProvider/MegaNz.h
    CloudProvider/OneDrive.h
    CloudProvider/PCloud.h
    CloudProvider/WebDav.h
    CloudProvider/YandexDisk.h
    Request/AuthorizeRequest.h
    Request/CreateDirectoryRequest.h
    Request/DeleteItemRequest.h
    Request/DownloadFileRequest.h
    Request/ExchangeCodeRequest.h
    Request/GetItemDataRequest.h
    Request/GetItemRequest.h
    Request/GetItemUrlRequest.h
    Request/HttpCallback.h
    Request/ListDirectoryPageRequest.h
    Request/ListDirectoryRequest.h
    Request/MoveItemRequest.h
    Request/RecursiveRequest.h
    Request/RenameItemRequest.h
    Request/Request.h
    Request/UploadFileRequest.h
    ${cloudstorage_PUBLIC_HEADERS}
)

set(cloudstorage_SOURCES
    CloudProvider/AmazonS3.cpp
    CloudProvider/AnimeZone.cpp
    CloudProvider/Box.cpp
    CloudProvider/CloudProvider.cpp
    CloudProvider/Dropbox.cpp
    CloudProvider/FourShared.cpp
    CloudProvider/GoogleDrive.cpp
    CloudProvider/GooglePhotos.cpp
    CloudProvider/HubiC.cpp
    CloudProvider/LocalDrive.cpp
    CloudProvider/LocalDriveWinRT.cpp
    CloudProvider/MegaNz.cpp
    CloudProvider/OneDrive.cpp
    CloudProvider/PCloud.cpp
    CloudProvider/WebDav.cpp
    CloudProvider/YandexDisk.cpp
    Request/AuthorizeRequest.cpp
    Request/CreateDirectoryRequest.cpp
    Request/DeleteItemRequest.cpp
    Request/DownloadFileRequest.cpp
    Request/ExchangeCodeRequest.cpp
    Request/GetItemDataRequest.cpp
    Request/GetItemRequest.cpp
    Request/GetItemUrlRequest.cpp
    Request/HttpCallback.cpp
    Request/ListDirectoryPageRequest.cpp
    Request/ListDirectoryRequest.cpp
    Request/MoveItemRequest.cpp
    Request/RecursiveRequest.cpp
    Request/RenameItemRequest.cpp
    Request/Request.cpp
    Request/UploadFileRequest.cpp
)

target_sources(cloudstorage PRIVATE
    ${cloudstorage_HEADERS}
    ${cloudstorage_SOURCES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${cloudstorage_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${cloudstorage_SOURCES})

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
set_target_properties(cloudstorage-util
    PROPERTIES
        PUBLIC_HEADER "${cloudstorage-util_PUBLIC_HEADERS}"
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

target_link_libraries(cloudstorage PRIVATE Threads::Threads cloudstorage-c-api cloudstorage-util)

cloudstorage_target_link_library(cloudstorage jsoncpp)
cloudstorage_target_link_library(cloudstorage-c-api jsoncpp)
cloudstorage_target_link_library(cloudstorage-util jsoncpp)

cloudstorage_find_library(tinyxml2 INCLUDE tinyxml2.h NAMES tinyxml2 DEBUG_NAMES tinyxml2d)
cloudstorage_target_link_library(cloudstorage tinyxml2)

if(curl_LIBRARY)
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(cloudstorage-util PRIVATE CURL_STATICLIB)
    endif()
    target_compile_definitions(cloudstorage-util PUBLIC WITH_CURL)
    cloudstorage_target_link_library(cloudstorage-util curl)
endif()

if(microhttpd_LIBRARY)
    target_compile_definitions(cloudstorage-util PUBLIC WITH_MICROHTTPD)
    cloudstorage_target_link_library(cloudstorage-util microhttpd)
endif()

if(ffmpeg_FOUND)
    target_compile_definitions(cloudstorage-util PUBLIC WITH_THUMBNAILER)
    cloudstorage_target_link_library(cloudstorage-util ffmpeg)
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
    target_compile_definitions(cloudstorage-util PUBLIC WITH_CRYPTOPP)
    cloudstorage_target_link_library(cloudstorage-util cryptopp)
endif()

target_include_directories(cloudstorage PUBLIC ".")
target_include_directories(cloudstorage-c-api PUBLIC ".")
target_include_directories(cloudstorage-util PUBLIC "." ${CMAKE_CURRENT_BINARY_DIR})

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
target_compile_definitions(cloudstorage-util PUBLIC UNICODE)

if(MSVC)
    target_compile_options(cloudstorage PRIVATE /wd4996 /wd4251 /wd4275 /Zc:__cplusplus /EHsc)
    target_compile_options(cloudstorage-c-api PRIVATE /wd4996 /wd4251 /wd4275 /Zc:__cplusplus /EHsc)
    target_compile_options(cloudstorage-util PRIVATE /wd4996 /wd4251 /wd4275 /Zc:__cplusplus /EHsc)
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
    target_compile_definitions(cloudstorage-util PRIVATE CLOUDSTORAGE_LIBRARY)
else()
    target_compile_definitions(cloudstorage PUBLIC CLOUDSTORAGE_STATIC_LINK)
    target_compile_definitions(cloudstorage-c-api PUBLIC CLOUDSTORAGE_STATIC_LINK)
    target_compile_definitions(cloudstorage-util PUBLIC CLOUDSTORAGE_STATIC_LINK)
endif()

install(TARGETS cloudstorage PUBLIC_HEADER DESTINATION include/cloudstorage)
install(TARGETS cloudstorage-c-api PUBLIC_HEADER DESTINATION include/cloudstorage/C)
install(TARGETS cloudstorage-util PUBLIC_HEADER DESTINATION include/cloudstorage/Utility)
