cmake_minimum_required(VERSION 3.10)

project(cloudstorage-tools)

add_subdirectory(resource-generator)

export(TARGETS resource-generator FILE ${CMAKE_BINARY_DIR}/ImportExecutables.cmake)
