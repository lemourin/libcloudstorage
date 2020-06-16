include(GoogleTest)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(googletest EXCLUDE_FROM_ALL)

add_executable(cloudstorage-test)

target_sources(cloudstorage-test PRIVATE
    CloudProvider/CloudProviderTest.cpp
    CloudProvider/GoogleDriveTest.cpp
)

set_target_properties(cloudstorage-test
    PROPERTIES
        CXX_STANDARD 17
)

target_include_directories(cloudstorage-test PRIVATE ".")

target_link_libraries(cloudstorage-test PRIVATE gtest gmock gtest_main cloudstorage)
cloudstorage_target_link_library(cloudstorage-test jsoncpp)

gtest_discover_tests(cloudstorage-test)

if(TOKEN_FILE)
    target_compile_definitions(cloudstorage-test PRIVATE TOKEN_FILE=${TOKEN_FILE})
endif()
