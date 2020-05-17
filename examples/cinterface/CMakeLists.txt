add_executable(cinterface)

target_sources(cinterface PRIVATE main.c)

target_link_libraries(cinterface PRIVATE cloudstorage)
target_include_directories(cinterface PRIVATE ${CMAKE_SOURCE_DIR}/src/C)