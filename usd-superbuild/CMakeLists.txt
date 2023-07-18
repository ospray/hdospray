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

project(USD_superbuild)

include(ExternalProject)
include(GNUInstallDirs)
include(ProcessorCount)
include(macros_and_options)

option(USE_PYTHON "enable python support" ON)
option(USE_PYTHON2 "if USE_PYTHON enabled, use python2 instead of python3" OFF)

###############################################################################
#########                            Options                   ################
###############################################################################

option(BUILD_ALL_DEPS "build ALL dependencies" OFF)
option(BUILD_GLEW "build glew"                OFF)
option(BUILD_TBB "build TBB"                  OFF)
option(BUILD_PNG "build libpng"               OFF)
option(BUILD_ZLIB "build zlib"                ON)
option(BUILD_JPEG "build libjpeg"             OFF)
option(BUILD_TIFF "build libtiff"             OFF)
option(BUILD_VDB "build OpenVDB"              OFF)
option(BUILD_BOOST "build BOOST"              ON)
option(BUILD_OPENEXR "build OpenEXR"          OFF)
option(BUILD_OPENIMAGEIO "build OpenImageIO"  OFF)
option(BUILD_HDF5 "build HDF5"                ON)
option(BUILD_OPENCOLORIO "build OpenColorIO"  OFF)
option(BUILD_PTEX        "build Ptex"         OFF)
option(BUILD_OPENSUBDIV  "build OpenSubdiv"   ON)
option(BUILD_USD "Build USD" ON)
set(USD_VERSION "v23.02" CACHE STRING "USD version tag")
set(PYTHON_VERSION "" CACHE STRING "python version, eg 3.9")

if(BUILD_ALL_DEPS)
  set(BUILD_GLEW 	ON)
  set(BUILD_PNG 	ON)
  set(BUILD_ZLIB 	ON)
  set(BUILD_JPEG 	ON)
  set(BUILD_TIFF 	ON)
  set(BUILD_TBB         ON)
  set(BUILD_VDB         ON)
  set(BUILD_BOOST       ON)
  set(BUILD_OPENEXR     ON)
  set(BUILD_OPENIMAGEIO ON)
  set(BUILD_HDF5        ON)
  set(BUILD_OPENCOLORIO ON)
  set(BUILD_PTEX        ON)
  set(BUILD_OPENSUBDIV  ON)
endif()


###############################################################################
#########          USD PXR OPTIONS                             ################
###############################################################################

# option(PXR_STRICT_BUILD_MODE "Turn on additional warnings. Enforce all warnings as errors." OFF)
# option(PXR_VALIDATE_GENERATED_CODE "Validate script generated code" OFF)
# option(PXR_HEADLESS_TEST_MODE "Disallow GUI based tests, useful for running under headless CI systems." OFF)
# option(PXR_BUILD_TESTS "Build tests" ON)
# option(PXR_BUILD_EXAMPLES "Build examples" ON)
# option(PXR_BUILD_TUTORIALS "Build tutorials" ON)
# option(PXR_BUILD_USD_TOOLS "Build commandline tools" ON)
# option(PXR_BUILD_IMAGING "Build imaging components" ON)
# option(PXR_BUILD_EMBREE_PLUGIN "Build embree imaging plugin" OFF)
option(PXR_BUILD_OPENIMAGEIO_PLUGIN "Build OpenImageIO plugin" OFF)
option(PXR_BUILD_OPENCOLORIO_PLUGIN "Build OpenColorIO plugin" OFF)
option(PXR_BUILD_USD_IMAGING "Build USD imaging components" ON)
option(PXR_BUILD_USDVIEW "Build usdview" ON)
# option(PXR_BUILD_ALEMBIC_PLUGIN "Build the Alembic plugin for USD" OFF)
# option(PXR_BUILD_DRACO_PLUGIN "Build the Draco plugin for USD" OFF)
option(PXR_BUILD_PRMAN_PLUGIN "Build the PRMan imaging plugin" OFF)
# option(PXR_BUILD_MATERIALX_PLUGIN "Build the MaterialX plugin for USD" OFF)
# option(PXR_BUILD_DOCUMENTATION "Generate doxygen documentation" OFF)
# option(PXR_ENABLE_GL_SUPPORT "Enable OpenGL based components" ON)
# option(PXR_ENABLE_PYTHON_SUPPORT "Enable Python based components for USD" ON)
# option(PXR_USE_PYTHON_3 "Build Python bindings for Python 3" OFF)
# option(PXR_ENABLE_HDF5_SUPPORT "Enable HDF5 backend in the Alembic plugin for USD" ON)
# option(PXR_ENABLE_OSL_SUPPORT "Enable OSL (OpenShadingLanguage) based components" OFF)
option(PXR_ENABLE_PTEX_SUPPORT "Enable Ptex support" OFF)
option(PXR_ENABLE_OPENVDB_SUPPORT "Enable OpenVDB support" OFF)
# option(PXR_ENABLE_NAMESPACES "Enable C++ namespaces." ON)


###############################################################################
#########  UPDATE DEPENDENCY BUILDS BASED ON USD PXR OPTIONS   ################
###############################################################################

#if (NOT PXR_BUILD_OPENCOLORIO_PLUGIN)
#  set(BUILD_OPENCOLORIO OFF)
#endif()

#if (NOT PXR_ENABLE_PTEX_SUPPORT)
#  set(BUILD_PTEX OFF)
#endif()

#if (NOT PXR_ENABLE_OPENVDB_SUPPORT)
#  set(BUILD_VDB OFF)
#endif()


###############################################################################
#########  SETUP USD CMAKE                                     ################
###############################################################################

set(USD_DEPENDENCIES "")

set(USD_ARGS
    -DPXR_BUILD_USD_IMAGING=${PXR_BUILD_USD_IMAGING}
    -DPXR_BUILD_OPENIMAGEIO_PLUGIN=${PXR_BUILD_OPENIMAGEIO_PLUGIN}
    -DPXR_BUILD_OPENCOLORIO_PLUGIN=${PXR_BUILD_OPENCOLORIO_PLUGIN}
    -DPXR_ENABLE_OPENVDB_SUPPORT=${PXR_ENABLE_OPENVDB_SUPPORT}
    -DPXR_BUILD_USDVIEW=${PXR_BUILD_USDVIEW}
    #-DPXR_ENABLE_VULKAN_SUPPORT=ON
    -DPXR_ENABLE_PTEX_SUPPORT=${PXR_ENABLE_PTEX_SUPPORT}
    -DPXR_BUILD_PRMAN_PLUGIN=${PXR_BUILD_PRMAN_PLUGIN}
    -DPXR_BUILD_TESTS=OFF
    -DPXR_BUILD_TUTORIALS=OFF
    -DPXR_BUILD_EXAMPLES=OFF
    -DPython_EXECUTABLE=${PYTHON_EXECUTABLE}
    -DPython3_EXECUTABLE=${PYTHON_EXECUTABLE}
    -DPYSIDE_BIN_DIR=${PYSIDE_BIN_DIR}
    #-DPython_ROOT_DIR=/usr/local/Frameworks/Python.framework/Versions/3.9
    #-DBoost_PYTHON_LIBRARY=/usr/local/bin/python3.9

)

if (BUILD_GLEW)
  include(Glew)
	set(USD_ARGS ${USD_ARGS}
		-DGLEW_INCLUDE_DIR:PATH=${CMAKE_INSTALL_PREFIX}/include
		-DGLEW_LIBRARY:FILEPATH=${CMAKE_INSTALL_PREFIX}/lib/libGLEW.so
	)
endif()

if (BUILD_ZLIB)
  include(zlib)
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} zlib)
endif()

## order defined below is crucial for setting up the cmake environment correctly
if (BUILD_TBB)
  include(TBB)
  set(USD_ARGS ${USD_ARGS}
    -DTBB_ROOT=${CMAKE_INSTALL_PREFIX}
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} tbb)
endif()
#Boost should have a TBB dependency (check that)
message("boost?")
if (BUILD_BOOST)
  message("boost?" yes)
  include(Boost)
  set(USD_ARGS ${USD_ARGS}
    -DBOOST_ROOT=${CMAKE_INSTALL_PREFIX}
    -DBoost_NO_SYSTEM_PATHS=ON
    -DBoost_NO_BOOST_CMAKE=ON
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} boost)
endif()

if (BUILD_PNG)
  include(png)
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} png)
endif()

if (BUILD_JPEG)
  include(jpeg)
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} jpeg)
endif()

if (BUILD_TIFF)
  include(tiff)
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} tiff)
endif()

if (BUILD_OPENEXR)
  include(OpenEXR)
  set(USD_ARGS ${USD_ARGS}
    -DOPENEXR_LOCATION=${CMAKE_INSTALL_PREFIX}
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} OpenEXR)
endif()

if(BUILD_OPENCOLORIO)
  include(OpenColorIO)
  set(USD_ARGS ${USD_ARGS}
    -DOCIO_LOCATION=${CMAKE_INSTALL_PREFIX}
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} OCIO)
endif()

if (BUILD_HDF5)
  include(hdf5)
  set(USD_ARGS ${USD_ARGS}
    -DOIIO_LOCATION=${CMAKE_INSTALL_PREFIX}
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} hdf5)
endif()

if (BUILD_OPENIMAGEIO)
  include(OpenImageIO)
  set(USD_ARGS ${USD_ARGS}
    -DOIIO_LOCATION=${CMAKE_INSTALL_PREFIX}
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} OIIO)
endif()

if(BUILD_PTEX)
  include(Ptex)
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} Ptex)
endif()

if(BUILD_OPENSUBDIV)
  include(OpenSubdiv)
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} OpenSubdiv)
endif()

if (BUILD_VDB)
  include(OpenVDB)
  set(USD_ARGS ${USD_ARGS}
    -DOPENVDB_LOCATION=${CMAKE_INSTALL_PREFIX}
  )
  set(USD_DEPENDENCIES ${USD_DEPENDENCIES} OpenVDB)
endif()

##
## USD
##

if (USE_PYTHON)
    set(USD_ARGS ${USD_ARGS} -DPXR_ENABLE_PYTHON_SUPPORT=ON " ")
    if (USE_PYTHON2)
      set(USD_ARGS ${USD_ARGS} -DPXR_USE_PYTHON_3=OFF " ")
    else()
      set(USD_ARGS ${USD_ARGS} -DPXR_USE_PYTHON_3=ON " ")
    endif()
else()
    set(USD_ARGS ${USD_ARGS} -DPXR_ENABLE_PYTHON_SUPPORT=OFF " ")
endif()


if (BUILD_USD)
  SET(EP_USD "USD")
  ExternalProject_Add (
    ${EP_USD}
    
    PREFIX        ${EP_USD}
    DOWNLOAD_DIR  ${EP_USD}
    STAMP_DIR     ${EP_USD}/stamp
    SOURCE_DIR    ${EP_USD}/source
    BINARY_DIR    ${EP_USD}/build
    GIT_REPOSITORY "https://github.com/PixarAnimationStudios/USD"
    GIT_TAG       ${USD_VERSION}
    GIT_SHALLOW   ON
    
    BUILD_ALWAYS  ON
    
    LIST_SEPARATOR | # Use the alternate list separator
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
      ${USD_ARGS}
    BUILD_ALWAYS OFF

    DEPENDS ${USD_DEPENDENCIES}
  )

  ExternalProject_Add_StepDependencies(${EP_USD}
        configure ${USD_DEPENDENCIES}
      )
  external_install(USD)
endif()

##
## CPACK
##

set(installed_directories ${installed_directories} "${CMAKE_INSTALL_PREFIX}" "install")
set(CPACK_INSTALLED_DIRECTORIES ${installed_directories})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)