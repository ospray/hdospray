## ======================================================================== ##
## Copyright 2009-2020 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

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

option(USE_PYTHON "enable python support" ON)
option(USE_PYTHON2 "if USE_PYTHON enabled, use python2 instead of python3" OFF)

set(SUPERBUILD_USD_VERSION "release-21.08" CACHE STRING "git tag of usd superbuild")
set(SUPERBUILD_USD_URL "https://github.com/carsonbrownlee/USD_superbuild.git"
  CACHE STRING "url of usd superbuild git")
option(OSPRAY_USE_GIT OFF)
option(OSPRAY_USE_EXTERNAL OFF)
if (OSPRAY_USE_EXTERNAL)
  set(OSPRAY_EXTERNAL_DIR "" CACHE PATH "location of external ospray install")
else()
  set(SUPERBUILD_OSPRAY_VERSION "v2.10.0" CACHE STRING "git tag of ospray")
  set(SUPERBUILD_OSPRAY_URL "https://github.com/ospray/ospray/archive/v2.10.0.zip"
    CACHE STRING "url of ospray git")
endif()
option(SUPERBUILD_USE_DENOISER "use ospray denoiser for OSPRay+hdOSPRay" ON)

###############################################################################
#########          Dependency Build Switches                   ################
###############################################################################

set(BUILD_USD ON CACHE BOOL "Build USD" )
option(BUILD_OSPRAY "Build OSPRay" ON)
option(BUILD_HDOSPRAY "Build HDOSPRAY" ON)
set(DOWNLOAD_TBB ON CACHE BOOL "download and build tbb")
set(TBB_PATH ${CMAKE_INSTALL_PREFIX} CACHE PATH "tbb root")

option(SUPERBUILD_USE_HOUDINI "build against existing houdini USD")
if (SUPERBUILD_USE_HOUDINI)
  set(SUPERBUILD_HOUDINI_DIR "/opt/hfs18.5/toolkit/cmake" CACHE PATH "path to houdini dir with HoudiniConfig.cmake")
  set(BUILD_USD OFF CACHE BOOL "Build USD" FORCE)
  set(TBB_PATH "${SUPERBUILD_HOUDINI_DIR}/../../dsolib/" CACHE PATH "tbb root" FORCE)
  set(DOWNLOAD_TBB OFF CACHE BOOL "download and build tbb" FORCE)
endif()
set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})


###############################################################################
#########  SETUP USD CMAKE                                     ################
###############################################################################

set(USD_DEPENDENCIES "")

set(USD_ARGS ""
)

if (BUILD_USD)
  include(ep_usd)
endif()
if (BUILD_OSPRAY)
  include(ep_ospray)
endif()

set(OSPRAY_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
if (OSPRAY_USE_EXTERNAL)
  set(OSPRAY_INSTALL_DIR ${OSPRAY_EXTERNAL_DIR})
endif()

if (BUILD_HDOSPRAY)
  
  set(EP_HDOSPRAY "hdOSPRay")
  set(USD_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
  if (NOT BUILD_USD)
    set(EXTERNAL_USD_DIR "" CACHE PATH "path to external USD install")   
    set(USD_ROOT_DIR ${EXTERNAL_USD_DIR})
  endif()

  set(OSPRAY_VERSION "2.10.0" CACHE STRING "ospray library version")
  set(RKCOMMON_VERSION "1.10.0" CACHE STRING "rkcommon library version")

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
      -DOIIO_BASE_DIR=${CMAKE_INSTALL_PREFIX}
      -DUSE_HOUDINI_USD=${SUPERBUILD_USE_HOUDINI}
      -DHoudini_DIR=${SUPERBUILD_HOUDINI_DIR}
      -DHDOSPRAY_ENABLE_DENOISER=${SUPERBUILD_USE_DENOISER}
  )
  ExternalProject_Add_StepDependencies(${EP_HDOSPRAY}
        configure ${EP_USD} ${EP_OSPRAY}
      )

endif()

###############################################################################
#########          Create Setup Script                         ################
###############################################################################

set(LD_EXPORT "LD_LIBRARY_PATH")
if (APPLE)
  set(LD_EXPORT "DYLD_LIBRARY_PATH")
endif()


if (SUPERBUILD_USE_HOUDINI)
  FILE(WRITE ${CMAKE_INSTALL_PREFIX}/setup_hdospray.sh 
  "export HDOSPRAY_ROOT=${CMAKE_INSTALL_PREFIX}\n"
  "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/lib:\${LD_LIBRARY_PATH}\n"
  "cd ${SUPERBUILD_HOUDINI_DIR}/../../\n"
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
