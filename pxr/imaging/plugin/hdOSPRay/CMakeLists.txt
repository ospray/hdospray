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
      material
      rendererPlugin
      renderDelegate
      renderPass
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

  find_package(ospray 2.2.0 REQUIRED)
  find_package(rkcommon 1.4.2 REQUIRED)

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
