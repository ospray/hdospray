# Copyright 2019 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

## Global CMake settings ##

cmake_minimum_required(VERSION 3.5)

SET (CMAKE_VERBOSE_MAKEFILE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT CMAKE_INSTALL_PREFIX)
  message(STATUS "CMAKE_INSTALL_PREFIX is not set using ${CMAKE_BINARY_DIR}/install")
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE STRING "Final install location." FORCE)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

## Establish project ##

project(hdospray_superbuild)

include(ExternalProject)
include(GNUInstallDirs)

macro(external_install NAME)
  ExternalProject_Get_property(${NAME} BINARY_DIR)
  set(binDir ${BINARY_DIR})
  set(projectInfo ${binDir} "${NAME}" "ALL" "/")
  set(depProjectInfo ${depProjectInfo} ${projectInfo})
endmacro()

option(USE_PYTHON "enable python support" ON)
option(USE_PYTHON2 "if USE_PYTHON enabled, use python2 instead of python3" OFF)

set(HDSUPER_USD_VERSION "v23.02" CACHE STRING "git tag of usd")
set(HDSUPER_USDSUPER_TAG "release-23.02" CACHE STRING "git tag of usd superbuild")
set(HDSUPER_USDSUPER_URL "https://github.com/carsonbrownlee/USD_superbuild.git"
  CACHE STRING "url of usd superbuild git")
option(HDSUPER_OSPRAY_USE_GIT OFF)
option(HDSUPER_OSPRAY_USE_EXTERNAL OFF)
if (HDSUPER_OSPRAY_USE_EXTERNAL)
  set(HDSUPER_OSPRAY_EXTERNAL_DIR "" CACHE PATH "location of external ospray install")
else()
  set(HDSUPER_OSPRAY_VERSION "v2.11.0" CACHE STRING "git tag of ospray")
  set(HDSUPER_OSPRAY_URL "https://github.com/ospray/ospray/archive/v2.11.0.zip"
    CACHE STRING "url of ospray git")
endif()
option(HDSUPER_USE_DENOISER "use ospray denoiser for OSPRay+hdOSPRay" ON)

###############################################################################
#########          Dependency Build Switches                   ################
###############################################################################

set(BUILD_USD ON CACHE BOOL "Build USD" )
option(BUILD_OSPRAY "Build OSPRay" ON)
option(BUILD_HDOSPRAY "Build HDOSPRAY" ON)
set(DOWNLOAD_TBB ON CACHE BOOL "download and build tbb")
set(TBB_PATH ${CMAKE_INSTALL_PREFIX} CACHE PATH "tbb root")

option(HDSUPER_USE_HOUDINI "build against existing houdini USD")
if (HDSUPER_USE_HOUDINI)
  set(HDSUPER_HOUDINI_DIR "/opt/hfs18.5/toolkit/cmake" CACHE PATH "path to houdini dir with HoudiniConfig.cmake")
  set(BUILD_USD OFF CACHE BOOL "Build USD" FORCE)
  set(TBB_PATH "${HDSUPER_HOUDINI_DIR}/../../dsolib/" CACHE PATH "tbb root" FORCE)
  set(DOWNLOAD_TBB OFF CACHE BOOL "download and build tbb" FORCE)
endif()
set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})


###############################################################################
#########  SETUP USD CMAKE                                     ################
###############################################################################

set(USD_DEPENDENCIES "")

set(USD_ARGS "")

if (BUILD_USD)
  include(ep_usd)
endif()
if (BUILD_OSPRAY)
  include(ep_ospray)
endif()

set(OSPRAY_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
if (HDSUOPER_OSPRAY_USE_EXTERNAL)
  set(OSPRAY_INSTALL_DIR ${OSPRAY_EXTERNAL_DIR})
endif()

if (BUILD_HDOSPRAY)
  
  set(EP_HDOSPRAY "hdOSPRay")
  set(USD_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
  if (NOT BUILD_USD)
    set(EXTERNAL_USD_DIR "" CACHE PATH "path to external USD install")   
    set(USD_ROOT_DIR ${EXTERNAL_USD_DIR})
  endif()

  set(OSPRAY_VERSION "2.11.0" CACHE STRING "ospray library version")
  set(RKCOMMON_VERSION "1.11.0" CACHE STRING "rkcommon library version")

  ExternalProject_Add (
    ${EP_HDOSPRAY}
    PREFIX ${EP_HDOSPRAY}
    DOWNLOAD_COMMAND ""
    STAMP_DIR ${EP_HDOSPRAY}/stamp
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../
    BINARY_DIR ${EP_HDOSPRAY}/build
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_PREFIX_PATH:PATH=${CMAKE_INSTALL_PREFIX}
      -DUSE_HOUDINI_USD=OFF
      -Dpxr_DIR=${USD_ROOT_DIR}
      -Dospray_DIR=${OSPRAY_INSTALL_DIR}/lib/cmake/ospray-${OSPRAY_VERSION}
      -Drkcommon_DIR=${OSPRAY_INSTALL_DIR}/lib/cmake/rkcommon-${RKCOMMON_VERSION}
      -DUSE_HOUDINI_USD=${HDSUPER_USE_HOUDINI}
      -DHoudini_DIR=${HDSUPER_HOUDINI_DIR}
      -DHDOSPRAY_ENABLE_DENOISER=${HDSUPER_USE_DENOISER}
      -DHDOSPRAY_PLUGIN_PTEX=OFF
    DEPENDS ${EP_USD_SUPER} ${EP_OSPRAY}
  )
  ExternalProject_Add_StepDependencies(${EP_HDOSPRAY}
        configure ${EP_USD_SUPER} ${EP_OSPRAY}
      )

  ###############################################################################
  #########          Create Setup Script                         ################
  ###############################################################################

  set(LD_EXPORT "LD_LIBRARY_PATH")
  if (APPLE)
    set(LD_EXPORT "DYLD_LIBRARY_PATH")
  endif()


  if (HDSUPER_USE_HOUDINI)
    FILE(WRITE ${CMAKE_INSTALL_PREFIX}/setup_hdospray.sh
    "export HDOSPRAY_ROOT=${CMAKE_INSTALL_PREFIX}\n"
    "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/lib:\${LD_LIBRARY_PATH}\n"
    "cd ${HDSUPER_HOUDINI_DIR}/../../\n"
    "source houdini_setup_bash\n"
    "cd -\n"
    "export PXR_PLUGINPATH_NAME=\${HDOSPRAY_ROOT}/plugin/usd/hdOSPRay/resources:\${PXR_PLUGINPATHNAME}\n"
    )
  else()
    FILE(WRITE ${CMAKE_INSTALL_PREFIX}/setup_hdospray.sh
    "export HDOSPRAY_ROOT=${CMAKE_INSTALL_PREFIX}\n"
    "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/lib:\${LD_LIBRARY_PATH}\n"
    "export PYTHONPATH=\${HDOSPRAY_ROOT}/lib/python:\${PYTHONPATH}\n"
    "export PATH=\${HDOSPRAY_ROOT}/bin:\${PATH}\n"
    "export PXR_PLUGINPATH_NAME=\${HDOSPRAY_ROOT}/plugin/usd/hdOSPRay/resources:\${PXR_PLUGINPATHNAME}\n"
    )
  endif()

  ExternalProject_Get_property(hdOSPRay BINARY_DIR)
  set(hdOSPRayBinaryDir ${BINARY_DIR})
  # build directory; name; components; destination dir in package
  set(hdOSPRayProjectInfo ${hdOSPRayBinaryDir} "hdOSPRay" "ALL" "/")
  set(depProjectInfo ${depProjectInfo} ${hdOSPRayProjectInfo})
  message("cpack projects: " ${depProjectInfo})
  set(CPACK_INSTALL_CMAKE_PROJECTS ${depProjectInfo})

endif(BUILD_HDOSPRAY)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)