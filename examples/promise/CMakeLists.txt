add_executable(promise)

target_sources(promise PRIVATE main.cpp)
target_link_libraries(promise PRIVATE cloudstorage)

set_target_properties(promise
    PROPERTIES
        CXX_STANDARD 17
)
