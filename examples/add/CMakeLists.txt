cmake_minimum_required(VERSION 3.14)
project(libthreadpool-add LANGUAGES CXX)

include("../../cmake/utils.cmake")
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

if(is_top_level)
    find_package(libthreadpool REQUIRED)
endif()

set(sources main.cpp)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})

add_executable(libthreadpool-add)
target_sources(libthreadpool-add PRIVATE ${sources})
target_link_libraries(libthreadpool-add PRIVATE libthreadpool::libthreadpool)

if(NOT is_top_level)
    win_copy_deps_to_target_dir(libthreadpool-add libthreadpool::libthreadpool)
endif()
