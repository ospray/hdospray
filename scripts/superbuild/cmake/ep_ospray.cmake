set(ARGS_OSPRAY_DOWNLOAD URL ${HDSUPER_OSPRAY_URL})
  if (HDSUPER_OSPRAY_USE_GIT)
    set(ARGS_OSPRAY_DOWNLOAD 
    GIT_REPOSITORY ${HDSUPER_OSPRAY_URL}
    GIT_TAG ${HDSUPER_OSPRAY_VERSION})
  endif()

  set(ENV{TBB_DIR} ${CMAKE_INSTALL_PREFIX}/tbb)
  set(ENV{TBB_ROOT} ${CMAKE_INSTALL_PREFIX}/tbb)

  # ospray needs different tbb version for build, but will break USD build
  # this ugly piece of code copies all ospray libs into temp folder and renames tbb libs
  if (WIN32)
    set(OSPRAY_INSTALL_COMMAND "")
  else()
    set(OSPRAY_INSTALL_COMMAND
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/ospray/lib/ ${CMAKE_INSTALL_PREFIX}/ospray_tmp
      COMMAND "${CMAKE_COMMAND}" -E rm -f ${CMAKE_INSTALL_PREFIX}/ospray_tmp/libtbb${CMAKE_SHARED_LIBRARY_SUFFIX}
      COMMAND "${CMAKE_COMMAND}" -E rm -f ${CMAKE_INSTALL_PREFIX}/ospray_tmp/libtbbmalloc${CMAKE_SHARED_LIBRARY_SUFFIX}
      COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different ${CMAKE_INSTALL_PREFIX}/ospray/include/ ${CMAKE_INSTALL_PREFIX}/include
      COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different ${CMAKE_INSTALL_PREFIX}/oidn/include/ ${CMAKE_INSTALL_PREFIX}/include
      COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different ${CMAKE_INSTALL_PREFIX}/rkcommon/include/ ${CMAKE_INSTALL_PREFIX}/include
      COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different ${CMAKE_INSTALL_PREFIX}/ospray_tmp ${CMAKE_INSTALL_PREFIX}/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/oidn/lib/ ${CMAKE_INSTALL_PREFIX}/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/rkcommon/lib/ ${CMAKE_INSTALL_PREFIX}/lib
      )
  endif()

  message("BUILD ISPC? ${BUILD_OSPRAY_ISPC}")
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
      -DDOWNLOAD_ISPC=${BUILD_OSPRAY_ISPC}
      -DISPC_VERSION=1.19.0
      -DDOWNLOAD_TBB=ON
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DOSPRAY_ENABLE_MODULES=ON
      -DBUILD_OSPRAY_APPS=OFF
      -DBUILD_OIDN=${HDSUPER_USE_DENOISER}
      -DBUILD_OIDN_FROM_SOURCE=OFF
      -DBUILD_EMBREE_FROM_SOURCE=OFF
      -DTBB_PATH=/Users/cbrownle/git/build-hdospray-superbuild/install/tbb
      -DEMBREE_TBB_ROOT=/Users/cbrownle/git/build-hdospray-superbuild/install/tbb
      -DTBB_INCLUDE_DIR=/Users/cbrownle/git/build-hdospray-superbuild/install/tbb/include
    INSTALL_COMMAND ${OSPRAY_INSTALL_COMMAND}
    DEPENDS ${EP_USD}
  )

  #ExternalProject_Add_StepDependencies(${EP_OSPRAY}
  #  configure USD
  #)

#external_install(OSPRay)
