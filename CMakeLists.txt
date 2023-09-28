cmake_minimum_required(VERSION 3.5)

project(threadpoollib VERSION 0.1.8 LANGUAGES C CXX)

set(SOURCES
    src/threadpoollib.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED On)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

export(TARGETS ${PROJECT_NAME} FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.cmake)