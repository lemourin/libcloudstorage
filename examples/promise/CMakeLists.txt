add_executable(promise)

target_sources(promise PRIVATE main.cpp)
target_link_libraries(promise PRIVATE cloudstorage)