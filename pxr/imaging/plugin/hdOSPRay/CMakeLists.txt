set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdOSPRay)
add_custom_target(shared_libs)

option(PXR_BUILD_OSPRAY_PLUGIN "build Intel hdOSPRay rendering backend" ON)

if (PXR_BUILD_OSPRAY_PLUGIN)
  find_package(ospray 1.8 CONFIG REQUIRED)

  # add ptex module to ospray libraries
  option(HDOSPRAY_PLUGIN_PTEX "build hdOSPRay with Ptex support")
  if (HDOSPRAY_PLUGIN_PTEX)
    add_definitions(-DHDOSPRAY_PLUGIN_PTEX)
    LINK_DIRECTORIES(${OSPRAY_ROOT}/lib64 ${OSPRAY_ROOT}/lib)
    SET(OSPRAY_MODULE_LIBRARIES ${OSPRAY_MODULE_LIBRARIES} ospray_module_ptex)
  endif()

  if(NOT PXR_BUILD_OPENIMAGEIO_PLUGIN)
    message(FATAL_ERROR "hdOSPRayPlugin requires PXR_BUILD_OPENIMAGEIO_PLUGIN")
  endif()

  set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/pxr/imaging/plugin/hdOSPRay/cmake)

  option(HDOSPRAY_ENABLE_DENOISER "build OSPRay plugin using OpenImageDenoise")
  if (HDOSPRAY_ENABLE_DENOISER)
    find_package(OpenImageDenoise REQUIRED)
    add_definitions(-DHDOSPRAY_ENABLE_DENOISER)
  endif()

  pxr_plugin(hdOSPRay
    LIBRARIES
      plug
      tf
      vt
      gf
      work
      hf
      hd
      hdx
      ${TBB_LIBRARIES}
      ${OSPRAY_LIBRARIES}
      ${OSPRAY_MODULE_LIBRARIES}
      ${EMBREE_LIBRARIES}
      ${OPENIMAGEDENOISE_LIBRARIES}
      ${OIIO_LIBRARIES}

    INCLUDE_DIRS
      ${TBB_INCLUDE_DIRS}
      ${OSPRAY_INCLUDE_DIRS}
      ${OIIO_INCLUDE_DIRS}
      ${OPENIMAGEDENOISE_INCLUDE_DIRS}

    PUBLIC_CLASSES
      config
      instancer
      mesh
      material
      rendererPlugin
      renderDelegate
      renderPass
      sampler

    PUBLIC_HEADERS
      context.h
      renderParam.h

    RESOURCE_FILES
      plugInfo.json
    )

endif()
