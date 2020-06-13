cmake_minimum_required(VERSION 3.10)

project(cloudstorage)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

set(CMAKE_PREFIX_PATH ${PREFIX_PATH_RELEASE} ${PREFIX_PATH_DEBUG} ${CMAKE_PREFIX_PATH})

if(WIN32)
    set(DEFAULT_BUILD_SHARED_LIBS OFF)
else()
    set(DEFAULT_BUILD_SHARED_LIBS ON)
endif()
option(BUILD_SHARED_LIBS "whether to build shared libraries" ${DEFAULT_BUILD_SHARED_LIBS})

function(cloudstorage_find_library LIBRARY)
    cmake_parse_arguments(CLOUDSTORAGE_LIBRARY
        ""
        "INCLUDE"
        "LIBRARY_PATH_SUFFIXES;INCLUDE_PATH_SUFFIXES;NAMES;DEBUG_NAMES"
        ${ARGN})

    find_library(${LIBRARY}_LIBRARY
        NAMES ${CLOUDSTORAGE_LIBRARY_NAMES}
        PATH_SUFFIXES ${CLOUDSTORAGE_LIBRARY_LIBRARY_PATH_SUFFIXES})
    set(${LIBRARY}_LIBRARY ${${LIBRARY}} PARENT_SCOPE)

    if(NOT CLOUDSTORAGE_LIBRARY_DEBUG_NAMES)
        set(CLOUDSTORAGE_LIBRARY_DEBUG_NAMES ${CLOUDSTORAGE_LIBRARY_NAMES})
    endif()

    find_library(${LIBRARY}_LIBRARY_DEBUG
        NAMES ${CLOUDSTORAGE_LIBRARY_DEBUG_NAMES}
        PATHS ${PREFIX_PATH_DEBUG}/lib ${QT_HOST_DATA}
        PATH_SUFFIXES ${CLOUDSTORAGE_LIBRARY_LIBRARY_PATH_SUFFIXES}
        NO_DEFAULT_PATH)
    set(${LIBRARY}_LIBRARY_DEBUG ${${LIBRARY}_DEBUG} PARENT_SCOPE)

    if(CLOUDSTORAGE_LIBRARY_INCLUDE)
        find_path(${LIBRARY}_INCLUDE_DIR
            ${CLOUDSTORAGE_LIBRARY_INCLUDE}
            PATH_SUFFIXES ${CLOUDSTORAGE_LIBRARY_INCLUDE_PATH_SUFFIXES})
        set(${LIBRARY}_INCLUDE_DIR ${${LIBRARY}_INCLUDE_DIR} PARENT_SCOPE)
    endif()
endfunction()

function(cloudstorage_target_link_library TARGET LIBRARY)
    if(${LIBRARY} STREQUAL "ffmpeg")
        cloudstorage_target_link_library(${TARGET} avcodec)
        cloudstorage_target_link_library(${TARGET} avfilter)
        cloudstorage_target_link_library(${TARGET} avformat)
        cloudstorage_target_link_library(${TARGET} avutil)
        cloudstorage_target_link_library(${TARGET} swresample)
        cloudstorage_target_link_library(${TARGET} swscale)
        return()
    endif()
    if(${LIBRARY}_LIBRARY_DEBUG)
        target_link_libraries(${TARGET} PRIVATE
            optimized ${${LIBRARY}_LIBRARY}
            debug ${${LIBRARY}_LIBRARY_DEBUG}
        )
    else()
        target_link_libraries(${TARGET} PRIVATE ${${LIBRARY}_LIBRARY})
    endif()
    target_include_directories(${TARGET} PRIVATE ${${LIBRARY}_INCLUDE_DIR})
endfunction()

find_package(Threads REQUIRED)

cloudstorage_find_library(jsoncpp INCLUDE_PATH_SUFFIXES jsoncpp INCLUDE json/json.h NAMES jsoncpp)
cloudstorage_find_library(curl INCLUDE curl/curl.h NAMES libcurl curl DEBUG_NAMES libcurl-d)
cloudstorage_find_library(microhttpd INCLUDE microhttpd.h NAMES microhttpd libmicrohttpd DEBUG_NAMES libmicrohttpd_d)

cloudstorage_find_library(avcodec INCLUDE libavcodec/avcodec.h NAMES avcodec)
cloudstorage_find_library(avfilter INCLUDE libavfilter/avfilter.h NAMES avfilter)
cloudstorage_find_library(avformat INCLUDE libavformat/avformat.h NAMES avformat)
cloudstorage_find_library(avutil INCLUDE libavutil/avutil.h NAMES avutil)
cloudstorage_find_library(swresample INCLUDE libswresample/swresample.h NAMES swresample)
cloudstorage_find_library(swscale INCLUDE libswscale/swscale.h NAMES swscale)

if(avcodec_LIBRARY AND avfilter_LIBRARY AND avformat_LIBRARY AND avutil_LIBRARY AND swresample_LIBRARY AND swscale_LIBRARY)
    set(ffmpeg_FOUND true)
endif()

if(NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tools)
else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/build-tools)
    execute_process(COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/build-tools ${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools)
    if(CMAKE_CONFIGURATION_TYPES)
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            execute_process(COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/build-tools ${CMAKE_COMMAND} --build . --config ${CONFIG})
        endforeach()
    else()
        execute_process(COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/build-tools ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE})
    endif()
    include(${CMAKE_CURRENT_BINARY_DIR}/build-tools/ImportExecutables.cmake)
endif()

if(curl_LIBRARY AND microhttpd_LIBRARY)
    add_subdirectory(src)
    add_subdirectory(bin)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
        add_subdirectory(examples)
    endif()
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
        add_subdirectory(test)
    endif()
endif()
