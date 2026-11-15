cmake_minimum_required(VERSION 3.23)
project(ACFConsumer LANGUAGES CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(../.. acf-build)
add_executable(consumer main.cpp)
# Deliberately no compiler/sanitizer options: consume the public target only.
target_link_libraries(consumer PRIVATE acf::acf)
