# Copyright 2019 Intel Corporation
# SPDX-License-Identifier: Apache-2.0


# Add a relative target path to the HDOSPRAY_RPATH.
function(_add_target_to_rpath CURRENT_RPATH target)  
  get_target_property(LIBRARY_FILE ${target} LOCATION)

  get_filename_component(LIBRARY_PATH ${LIBRARY_FILE} DIRECTORY)

  file(RELATIVE_PATH
      LIBRARY_PATH_REL
      "${CMAKE_INSTALL_PREFIX}/plugin/usd"
      ${LIBRARY_PATH}   
  )
  set (HDOSPRAY_RPATH "${CURRENT_RPATH}:$ORIGIN/${LIBRARY_PATH_REL}" PARENT_SCOPE)
endfunction()


add_library(hdOSPRay SHARED
    config.cpp
    instancer.cpp
    mesh.cpp
    camera.cpp
    basisCurves.cpp
    material.cpp
    rendererPlugin.cpp
    renderDelegate.cpp
    renderPass.cpp
    renderBuffer.cpp
    sampler.cpp
    texture.cpp
    lights/light.cpp
    lights/diskLight.cpp
    lights/distantLight.cpp
    lights/domeLight.cpp
    lights/rectLight.cpp
    lights/sphereLight.cpp
    lights/cylinderLight.cpp
    context.h
    renderParam.h
    plugInfo.json
  )

find_package(ospray 2.5.0 REQUIRED)
find_package(rkcommon 1.6.0 REQUIRED)

if(USE_HOUDINI_USD)
  # we build hdOSPRAy against the USD libraries shipped
  # with Houdini 
  target_link_libraries(hdOSPRay PUBLIC
    ospray::ospray
    ospray::ospray_sdk
    rkcommon::rkcommon
    Houdini
    )
else()
  
  target_include_directories(hdOSPRay PUBLIC ${PXR_INCLUDE_DIRS})
  target_link_libraries(hdOSPRay PUBLIC
    ospray::ospray
    ospray::ospray_sdk
    rkcommon::rkcommon
    ${PXR_LIBRARIES}
    )

    # we need to find a way to figure out if OIIO is part of the USD package or not
    if (TRUE)
    find_package(OpenImageIO REQUIRED)
    target_include_directories(hdOSPRay PUBLIC ${OIIO_INCLUDE_DIRS})
    target_link_libraries(hdOSPRay PUBLIC
      ${OIIO_LIBRARIES}
      )
    endif()
endif()

target_compile_definitions(hdOSPRay
  PRIVATE
  MFB_PACKAGE_NAME=hdOSPRay
  MFB_ALT_PACKAGE_NAME=hdOSPRay
  )

install(TARGETS hdOSPRay DESTINATION plugin/usd)

# plugInfo.json
set(PLUG_INFO_LIBRARY_PATH "../hdOSPRay${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_ROOT "..")
configure_file(plugInfo.json plugInfo.json @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json" DESTINATION plugin/usd/hdOSPRay/resources)

target_include_directories(hdOSPRay PUBLIC ${OIIO_INCLUDE_DIRS})

# add ptex module to ospray libraries
option(HDOSPRAY_PLUGIN_PTEX "build hdOSPRay with Ptex support" OFF)
if (HDOSPRAY_PLUGIN_PTEX)
  if (NOT TARGET ospray::ospray_module_ptex)
    message(FATAL_ERROR "hdOSPRayPlugin requires building OSPRay with ptex module")
  endif()

  find_package(ptex CONFIG REQUIRED)
  target_link_libraries(hdOSPRay PUBLIC ospray::ospray_module_ptex)
  target_compile_definitions(hdOSPRay PUBLIC -DHDOSPRAY_PLUGIN_PTEX)
endif()

# add OpenImageDenoise
option(HDOSPRAY_ENABLE_DENOISER "build OSPRay plugin using OpenImageDenoise" OFF)
if (HDOSPRAY_ENABLE_DENOISER)
  find_package(OpenImageDenoise 1.2.1 CONFIG)
  if (NOT TARGET OpenImageDenoise)
    message(FATAL_ERROR "hdOSPRayPlugin requires OpenImageDenoise be installed")
  endif()

  target_link_libraries(hdOSPRay PUBLIC OpenImageDenoise)
  target_compile_definitions(hdOSPRay PUBLIC -DHDOSPRAY_ENABLE_DENOISER)
endif()

########################################################################
# RPATH
########################################################################

option(HDOSPRAY_SET_REL_RPATH "Adds the relative paths of OSPray and rkcommon to the rpath" ON)

set (HDOSPRAY_RPATH "$ORIGIN")
set (HDOSPRAY_RPATH "${HDOSPRAY_RPATH}:/../../lib")
if(USE_HOUDINI_USD)
else()
  set (HDOSPRAY_RPATH "${HDOSPRAY_RPATH}:${pxr_DIR}/lib")
endif()

if(HDOSPRAY_SET_REL_RPATH)
  _add_target_to_rpath(${HDOSPRAY_RPATH} ospray::ospray)
  _add_target_to_rpath(${HDOSPRAY_RPATH} rkcommon::rkcommon)
endif()

set_target_properties(hdOSPRay PROPERTIES
  PREFIX ""
  INSTALL_RPATH ${HDOSPRAY_RPATH}
  )
