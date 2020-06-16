add_executable(resource-generator)

target_sources(resource-generator PRIVATE main.cpp)

set_target_properties(resource-generator
    PROPERTIES
        CXX_STANDARD 17
)
