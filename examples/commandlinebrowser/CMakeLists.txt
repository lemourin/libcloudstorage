add_executable(commandlinebrowser)

target_sources(commandlinebrowser PRIVATE main.cpp)
cloudstorage_target_link_library(commandlinebrowser jsoncpp)
target_link_libraries(commandlinebrowser PRIVATE cloudstorage)

set_target_properties(commandlinebrowser
    PROPERTIES
        CXX_STANDARD 17
)
