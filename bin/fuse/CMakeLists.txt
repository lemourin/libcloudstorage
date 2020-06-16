add_executable(cloudstorage-fuse)

target_sources(cloudstorage-fuse PRIVATE
    FuseCommon.cpp
    FuseCommon.h
    FuseLowLevel.cpp
    FuseLowLevel.h
    FuseHighLevel.cpp
    FuseHighLevel.h
    FuseDokan.cpp
    FuseDokan.h
    FileSystem.cpp
    FileSystem.h
    IFileSystem.h
    FuseWinFsp.cpp
    FuseWinFsp.h
    main.cpp
)

set_target_properties(cloudstorage-fuse
    PROPERTIES
        CXX_STANDARD 17
)

target_link_libraries(cloudstorage-fuse PRIVATE cloudstorage Threads::Threads)

cloudstorage_target_link_library(cloudstorage-fuse jsoncpp)

if(fuse_LIBRARY)
    target_compile_definitions(cloudstorage-fuse PRIVATE WITH_FUSE)
    cloudstorage_target_link_library(cloudstorage-fuse fuse)
elseif(fuse-legacy_LIBRARY)
    target_compile_definitions(cloudstorage-fuse PRIVATE WITH_LEGACY_FUSE _FILE_OFFSET_BITS=64)
    cloudstorage_target_link_library(cloudstorage-fuse fuse-legacy)
elseif(winfsp_LIBRARY)
    target_compile_definitions(cloudstorage-fuse PRIVATE WITH_WINFSP WITH_LEGACY_FUSE)
    find_path(fuse-winfsp fuse.h PATHS ${winfsp_INCLUDE_DIR}/winfsp/fuse)

    cloudstorage_target_link_library(cloudstorage-fuse winfsp)
    target_include_directories(cloudstorage-fuse PRIVATE ${fuse-winfsp})

    target_link_libraries(cloudstorage-fuse PRIVATE version credui rpcrt4)
elseif(dokan2_LIBRARY)
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(cloudstorage-fuse PRIVATE _EXPORTING)
    endif()

    target_compile_definitions(cloudstorage-fuse PRIVATE WITH_DOKAN WITH_LEGACY_FUSE)
    cloudstorage_target_link_library(cloudstorage-fuse dokan2)

    cloudstorage_find_library(fuse-dokan INCLUDE_PATH_SUFFIXES dokan_fuse INCLUDE fuse.h NAMES dokanfuse2)
    cloudstorage_target_link_library(cloudstorage-fuse fuse-dokan)
endif()

install(TARGETS cloudstorage-fuse)
