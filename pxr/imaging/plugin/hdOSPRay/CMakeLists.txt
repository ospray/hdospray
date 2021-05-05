#
# Copyright 2021 Intel
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#

set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdOSPRay)
add_custom_target(shared_libs)

option(PXR_BUILD_OSPRAY_PLUGIN "build Intel hdOSPRay rendering backend" ON)

if (PXR_BUILD_OSPRAY_PLUGIN)

  find_package(OpenImageIO 1.8.9 REQUIRED)

  pxr_plugin(hdOSPRay
    PUBLIC_CLASSES
      config
      instancer
      mesh
      basisCurves
      material
      rendererPlugin
      renderDelegate
      renderPass
      renderBuffer
      sampler
      texture
      lights/light
      lights/diskLight
      lights/distantLight
      lights/domeLight
      lights/rectLight
      lights/sphereLight
      #lights/simpleLight

    PUBLIC_HEADERS
      context.h
      renderParam.h

    RESOURCE_FILES
      plugInfo.json
    )

  find_package(ospray 2.5.0 REQUIRED)
  find_package(rkcommon 1.6.0 REQUIRED)

  target_link_libraries(hdOSPRay PUBLIC
    plug
    tf
    vt
    gf
    work
    hf
    hd
    hdx
    ospray::ospray
    ospray::ospray_sdk
    rkcommon::rkcommon
    ${OPENEXR_LIBRARIES}
    ${OIIO_LIBRARIES}
    )

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

endif()
