set(ARGS_OSPRAY_DOWNLOAD URL ${HDSUPER_OSPRAY_URL})
  if (HDSUPER_OSPRAY_USE_GIT)
    set(ARGS_OSPRAY_DOWNLOAD 
    GIT_REPOSITORY ${HDSUPER_OSPRAY_URL}
    GIT_TAG ${HDSUPER_OSPRAY_VERSION})
  endif()

  set(ENV{TBB_DIR} ${INSTALL_PREFIX})
  set(ENV{TBB_DIR} ${INSTALL_PREFIX}/lib/cmake/tbb)

  set(OSPRAY_INSTALL_COMMAND
    COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/oidn/lib ${CMAKE_INSTALL_PREFIX}/lib
    COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/embree/lib ${CMAKE_INSTALL_PREFIX}/lib
    COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/rkcommon/lib ${CMAKE_INSTALL_PREFIX}/lib
    COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/openvkl/lib ${CMAKE_INSTALL_PREFIX}/lib
    )

  set(EP_OSPRAY "OSPRay")
  ExternalProject_Add (
    ${EP_OSPRAY}
    PREFIX ${EP_OSPRAY}
    ${ARGS_OSPRAY_DOWNLOAD}
    STAMP_DIR     ${EP_OSPRAY}/stamp
    SOURCE_DIR    ${EP_OSPRAY}/source
    BINARY_DIR    ${EP_OSPRAY}/build
    GIT_SHALLOW   1
    GIT_SUBMODULES "scripts"  # hack: setting submodules to empty enables all submodules
    #GIT_SUBMODULES_RECURSE 0
    CONFIGURE_COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/${EP_OSPRAY}/source/scripts/superbuild
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}
      -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
      -DINSTALL_IN_SEPARATE_DIRECTORIES=ON
      -DDOWNLOAD_ISPC=ON
      -DDOWNLOAD_TBB=OFF
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DOSPRAY_ENABLE_MODULES=ON
      -DBUILD_OSPRAY_APPS=OFF
      -DBUILD_OIDN=${HDSUPER_USE_DENOISER}
      -DBUILD_OIDN_FROM_SOURCE=${HDSUPER_USE_DENOISER}
      -DTBB_PATH=${TBB_PATH}
    INSTALL_COMMAND ${OSPRAY_INSTALL_COMMAND}
    DEPENDS ${EP_USD}
  )

  #ExternalProject_Add_StepDependencies(${EP_OSPRAY}
  #  configure USD
  #)

external_install(OSPRay)
