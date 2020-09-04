include(GoogleTest)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(googletest EXCLUDE_FROM_ALL)

add_executable(cloudstorage-test)

set(cloudstorage-testdata
    ${CMAKE_CURRENT_SOURCE_DIR}/data/mega_files_response.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/mega_login_response.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/mega_token.txt
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/data PREFIX "Test Files" FILES ${cloudstorage-testdata})

target_sources(cloudstorage-test PRIVATE
    CloudProvider/CloudProviderTest.cpp
    CloudProvider/GoogleDriveTest.cpp
    CloudProvider/MegaNzTest.cpp
    CloudProvider/WebDavTest.cpp
    Utility/RequestTest.cpp
    Utility/AuthMock.h
    Utility/HttpMock.h
    Utility/HttpServerMock.h
    Utility/CloudProviderMock.h
    Utility/CloudFactoryMock.h
    ${CMAKE_CURRENT_BINARY_DIR}/TestData.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/TestData.h
    ${cloudstorage-testdata}
)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/TestData.cpp ${CMAKE_CURRENT_BINARY_DIR}/TestData.h
    COMMAND resource-generator ${CMAKE_CURRENT_BINARY_DIR}/TestData ${cloudstorage-testdata}
    DEPENDS ${cloudstorage-testdata} resource-generator
)

set_target_properties(cloudstorage-test
    PROPERTIES
        CXX_STANDARD 17
)

target_include_directories(cloudstorage-test PRIVATE "." ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(cloudstorage-test PRIVATE gtest gmock gtest_main cloudstorage)
cloudstorage_target_link_library(cloudstorage-test jsoncpp)

gtest_discover_tests(cloudstorage-test)

if(TOKEN_FILE)
    target_compile_definitions(cloudstorage-test PRIVATE TOKEN_FILE=${TOKEN_FILE})
endif()
