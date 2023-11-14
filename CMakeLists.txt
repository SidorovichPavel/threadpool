cmake_minimum_required(VERSION 3.25)

project(threadpool VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED On)


set(SOURCES
    src/threadpool.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

export(TARGETS ${PROJECT_NAME} FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.cmake)

add_executable(test tests/test.cpp)
target_link_libraries(test threadpool)