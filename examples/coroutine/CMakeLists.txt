add_executable(coroutine)

target_sources(coroutine PRIVATE main.cpp)
target_link_libraries(coroutine PRIVATE cloudstorage)

set_target_properties(coroutine
    PROPERTIES
        CXX_STANDARD 20
)
