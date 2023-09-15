cmake_minimum_required(VERSION 3.14)
project(libthreadpool
    VERSION 1.0.0
    DESCRIPTION "Template "
    LANGUAGES CXX)

#----------------------------------------------------------------------------------------------------------------------
# general settings and options
#----------------------------------------------------------------------------------------------------------------------

include(cmake/utils.cmake)
include(GNUInstallDirs)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

# LIBTHREADPOOL_SHARED_LIBS option (undefined by default) can be used to force shared/static build
option(LIBTHREADPOOL_BUILD_TESTS "Build libthreadpool tests" OFF)
option(LIBTHREADPOOL_BUILD_EXAMPLES "Build libthreadpool examples" OFF)
option(LIBTHREADPOOL_BUILD_DOCS "Build libthreadpool documentation" OFF)
option(LIBTHREADPOOL_INSTALL "Generate target for installing libthreadpool" ${is_top_level})
set_if_undefined(LIBTHREADPOOL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/libthreadpool" CACHE STRING
    "Install path for libthreadpool package-related CMake files")

if(DEFINED LIBTHREADPOOL_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${LIBTHREADPOOL_SHARED_LIBS})
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set_if_undefined(CMAKE_CXX_VISIBILITY_PRESET hidden)
set_if_undefined(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(libthreadpool) # initialized below
add_library(libthreadpool::libthreadpool ALIAS libthreadpool)

#----------------------------------------------------------------------------------------------------------------------
# libthreadpool dependencies
#----------------------------------------------------------------------------------------------------------------------

# Search for your dependencies here

#----------------------------------------------------------------------------------------------------------------------
# libthreadpool sources
#----------------------------------------------------------------------------------------------------------------------

include(GenerateExportHeader)
set(export_file_name "export_shared.h")

if(NOT BUILD_SHARED_LIBS)
    set(export_file_name "export_static.h")
endif()

generate_export_header(libthreadpool EXPORT_FILE_NAME include/libthreadpool/${export_file_name})

set(sources
    include/libthreadpool/export.h
    include/libthreadpool/libthreadpool.hpp
    src/libthreadpool.cpp)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})

#----------------------------------------------------------------------------------------------------------------------
# libthreadpool target
#----------------------------------------------------------------------------------------------------------------------

include(CMakePackageConfigHelpers)

target_sources(libthreadpool PRIVATE ${sources})
target_compile_definitions(libthreadpool PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:LIBTHREADPOOL_STATIC_DEFINE>")

target_include_directories(libthreadpool
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include")

set_target_properties(libthreadpool PROPERTIES
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION})

set_property(TARGET libthreadpool PROPERTY CXX_STANDARD 23)

if(libthreadpool_INSTALL AND NOT CMAKE_SKIP_INSTALL_RULES)
    configure_package_config_file(cmake/libthreadpool-config.cmake.in libthreadpool-config.cmake
        INSTALL_DESTINATION "${LIBTHREADPOOL_INSTALL_CMAKEDIR}")

    write_basic_package_version_file(libthreadpool-config-version.cmake
        COMPATIBILITY SameMajorVersion)

    install(TARGETS libthreadpool EXPORT libthreadpool_export
        RUNTIME COMPONENT libthreadpool
        LIBRARY COMPONENT libthreadpool NAMELINK_COMPONENT libthreadpool-dev
        ARCHIVE COMPONENT libthreadpool-dev
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(DIRECTORY include/
        TYPE INCLUDE
        COMPONENT libthreadpool-dev)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/libthreadpool/${export_file_name}"
        COMPONENT libthreadpool-dev
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libthreadpool")

    set(targets_file "libthreadpool-shared-targets.cmake")

    if(NOT BUILD_SHARED_LIBS)
        set(targets_file "libthreadpool-static-targets.cmake")
    endif()

    install(EXPORT libthreadpool_export
        COMPONENT libthreadpool-dev
        FILE "${targets_file}"
        DESTINATION "${LIBTHREADPOOL_INSTALL_CMAKEDIR}"
        NAMESPACE libthreadpool::)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/libthreadpool-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/libthreadpool-config-version.cmake"
        COMPONENT libthreadpool-dev
        DESTINATION "${LIBTHREADPOOL_INSTALL_CMAKEDIR}")

    if(MSVC)
        set(pdb_file "")
        set(pdb_file_destination "")

        if(BUILD_SHARED_LIBS)
            set(pdb_file "$<TARGET_PDB_FILE:libthreadpool>")
            set(pdb_file_destination "${CMAKE_INSTALL_BINDIR}")
        else()
            # TARGET_PDB_FILE does not work for pdb file generated for static library build, determining it manually
            set(pdb_file "$<TARGET_FILE_DIR:libthreadpool>/$<TARGET_FILE_PREFIX:libthreadpool>$<TARGET_FILE_BASE_NAME:libthreadpool>.pdb")
            set(pdb_file_destination "${CMAKE_INSTALL_LIBDIR}")
        endif()

        install(FILES "${pdb_file}"
            COMPONENT libthreadpool-dev
            CONFIGURATIONS Debug RelWithDebInfo
            DESTINATION "${pdb_file_destination}"
            OPTIONAL)
    endif()
endif()

#----------------------------------------------------------------------------------------------------------------------
# other targets
#----------------------------------------------------------------------------------------------------------------------

if(LIBTHREADPOOL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LIBTHREADPOOL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LIBTHREADPOOL_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    doxygen_add_docs(docs include)
endif()