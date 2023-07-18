## Copyright 2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

#### Global superbuild options/vars ####

ProcessorCount(PROCESSOR_COUNT)

if(NOT PROCESSOR_COUNT EQUAL 0)
  set(BUILD_JOBS ${PROCESSOR_COUNT} CACHE STRING "Number of build jobs '-j <n>'")
else()
  set(BUILD_JOBS 4 CACHE STRING "Number of build jobs '-j <n>'")
endif()

option(INSTALL_IN_SEPARATE_DIRECTORIES
  "Install libraries into their own directories under CMAKE_INSTALL_PREFIX"
  OFF
)
mark_as_advanced(INSTALL_IN_SEPARATE_DIRECTORIES)

if(NOT CMAKE_INSTALL_PREFIX)
  set(installDir ${CMAKE_CURRENT_BINARY_DIR}/install)
else()
set(installDir ${CMAKE_INSTALL_PREFIX})
endif()

get_filename_component(INSTALL_DIR_ABSOLUTE
  ${installDir} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})

if(${CMAKE_VERSION} VERSION_GREATER 3.11.4)
  set(PARALLEL_JOBS_OPTS -j ${BUILD_JOBS})
endif()

set(DEFAULT_BUILD_COMMAND cmake --build . --config Release ${PARALLEL_JOBS_OPTS})

###############################################################################
###############################################################################
###############################################################################

#### Helper macros ####

macro(setup_component_path_vars _NAME _VERSION)
  set(COMPONENT_VERSION ${_VERSION})
  set(COMPONENT_NAME ${_NAME})
  set(COMPONENT_FULL_NAME ${_NAME}-${_VERSION})

  set(COMPONENT_INSTALL_PATH ${INSTALL_DIR_ABSOLUTE})
  if(INSTALL_IN_SEPARATE_DIRECTORIES)
    set(COMPONENT_INSTALL_PATH
        ${INSTALL_DIR_ABSOLUTE}/${COMPONENT_FULL_NAME})
  endif()

  set(COMPONENT_DOWNLOAD_PATH ${COMPONENT_FULL_NAME})
  set(COMPONENT_SOURCE_PATH ${COMPONENT_FULL_NAME}/source)
  set(COMPONENT_STAMP_PATH ${COMPONENT_FULL_NAME}/stamp)
  set(COMPONENT_BUILD_PATH ${COMPONENT_FULL_NAME}/build)
endmacro()

macro(append_cmake_prefix_path)
  list(APPEND CMAKE_PREFIX_PATH ${ARGN})
  string(REPLACE ";" "|" CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
endmacro()

macro(external_install NAME)
  ExternalProject_Get_property(${NAME} BINARY_DIR)
  #ExternalProject_Get_property(${NAME} INSTALL_DIR)
  set(binDir ${BINARY_DIR})
  set(projectInfo ${binDir} "${NAME}" "ALL" "/")
  set(depProjectInfo ${depProjectInfo} ${projectInfo})
endmacro()

#### Main component build macro ####

macro(build_component)
  # See cmake_parse_arguments docs to see how args get parsed here:
  #    https://cmake.org/cmake/help/latest/command/cmake_parse_arguments.html
  set(options CLONE_GIT_REPOSITORY OMIT_FROM_INSTALL INSTALL_BINARIES)
  set(oneValueArgs NAME VERSION URL)
  set(multiValueArgs BUILD_ARGS DEPENDS_ON)
  cmake_parse_arguments(BUILD_COMPONENT "${options}" "${oneValueArgs}"
                        "${multiValueArgs}" ${ARGN})

  # Setup COMPONENT_* variables (containing paths) for this function
  setup_component_path_vars(${BUILD_COMPONENT_NAME} ${BUILD_COMPONENT_VERSION})

  if(BUILD_COMPONENT_OMIT_FROM_INSTALL)
    #set(COMPONENT_SOURCE_PATH ${COMPONENT_FULL_NAME}/source)
    #set(COMPONENT_INSTALL_PATH ${CMAKE_BINARY_DIR}/${COMPONENT_NAME}/install)
  endif()

  # Setup where we get source from (clone repo or download source zip)
  if(BUILD_COMPONENT_CLONE_GIT_REPOSITORY)
    set(COMPONENT_REMOTE_SOURCE_OPTIONS
      GIT_REPOSITORY ${BUILD_COMPONENT_URL}
      GIT_TAG ${COMPONENT_VERSION}
      GIT_SHALLOW ON
    )
  else()
    set(COMPONENT_REMOTE_SOURCE_OPTIONS
      URL "${BUILD_COMPONENT_URL}/archive/${COMPONENT_VERSION}.zip"
    )
  endif()

  set(INSTALL_COMMAND
      COMMAND make install
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
      ${COMPONENT_INSTALL_PATH}/lib/
      ${installDir}/lib/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
      ${COMPONENT_INSTALL_PATH}/include
      ${installDir}/include
  )
  if (BUILD_COMPONENT_INSTALL_BINARIES)
      set(INSTALL_COMMAND ${INSTALL_COMMAND} COMMAND "${CMAKE_COMMAND}" -E copy_directory
      ${COMPONENT_INSTALL_PATH}/bin
      ${installDir}/bin)
  endif()


  # Build the actual component
  ExternalProject_Add(${COMPONENT_NAME}
    PREFIX ${COMPONENT_FULL_NAME}
    DOWNLOAD_DIR ${COMPONENT_DOWNLOAD_PATH}
    STAMP_DIR ${COMPONENT_STAMP_PATH}
    SOURCE_DIR ${COMPONENT_SOURCE_PATH}
    BINARY_DIR ${COMPONENT_BUILD_PATH}
    ${COMPONENT_REMOTE_SOURCE_OPTIONS}
    LIST_SEPARATOR | # Use the alternate list separator
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DCMAKE_INSTALL_PREFIX:PATH=${COMPONENT_INSTALL_PATH}
      -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
      -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
      -DCMAKE_INSTALL_DOCDIR=${CMAKE_INSTALL_DOCDIR}
      -DCMAKE_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
      -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
      ${BUILD_COMPONENT_BUILD_ARGS}
    BUILD_COMMAND ${DEFAULT_BUILD_COMMAND}
    #INSTALL_COMMAND ${INSTALL_COMMAND}
    BUILD_ALWAYS OFF
  )

  if(BUILD_COMPONENT_DEPENDS_ON)
    ExternalProject_Add_StepDependencies(${COMPONENT_NAME}
      configure ${BUILD_COMPONENT_DEPENDS_ON}
    )
  endif()

endmacro()
