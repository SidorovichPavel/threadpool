cmake_minimum_required(VERSION 3.5)

set(LIBRARY_NAME threadpoollib)

set(SOURCES
    src/threadpoollib.cpp
)

add_library(${LIBRARY_NAME} STATIC ${SOURCES})

target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

export(TARGETS ${LIBRARY_NAME} FILE ${CMAKE_BINARY_DIR}/threadpoollib.cmake)