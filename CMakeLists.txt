# SPDX-License-Identifier: Apache-2.0

PROJECT(hdOSPRayPlugin)

set(HDOSPRAY_VERSION_MAJOR 0)
set(HDOSPRAY_VERSION_MINOR 11)
set(HDOSPRAY_VERSION_PATCH 0)
set(HDOSPRAY_VERSION ${HDOSPRAY_VERSION_MAJOR}.${HDOSPRAY_VERSION_MINOR}.${HDOSPRAY_VERSION_PATCH})

if (WIN32)
    cmake_minimum_required(VERSION 3.1.1)
else()
    cmake_minimum_required(VERSION 2.8.8)
endif()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    if (WIN32)
        message(FATAL_ERROR "Compiler does not support 64-bit builds. "
                "If you are using Visual Studio, make sure you are in the "
                "x64 command prompt and have specified the Win64 cmake "
                "generator (e.g., -G \"Visual Studio 14 2015 Win64\")")
    else()
        message(FATAL_ERROR "Compiler does not support 64-bit builds.")
    endif()
endif()

set(CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/cmake
    ${CMAKE_SOURCE_DIR}/cmake/modules)


if(APPLE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()

include(macros)

add_subdirectory(hdOSPRay)
include(package)

option(ENABLE_TESTING "build testing suite" OFF)

if (ENABLE_TESTING)
    add_subdirectory(testing)
endif()

include(CPack)
