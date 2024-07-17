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
      COMMAND "${CMAKE_COMMAND}" -E remove_directory ${CMAKE_INSTALL_PREFIX}/ospray_tmp/libtbb${CMAKE_SHARED_LIBRARY_SUFFIX}
      COMMAND "${CMAKE_COMMAND}" -E remove_directory ${CMAKE_INSTALL_PREFIX}/ospray_tmp/libtbb.12.${CMAKE_SHARED_LIBRARY_SUFFIX}
      COMMAND "${CMAKE_COMMAND}" -E remove_directory ${CMAKE_INSTALL_PREFIX}/ospray_tmp/libtbbmalloc.12.${CMAKE_SHARED_LIBRARY_SUFFIX}
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/ospray/include/ ${CMAKE_INSTALL_PREFIX}/include
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/oidn/include/ ${CMAKE_INSTALL_PREFIX}/include
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/rkcommon/include/ ${CMAKE_INSTALL_PREFIX}/include
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/ospray_tmp ${CMAKE_INSTALL_PREFIX}/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/oidn/lib/ ${CMAKE_INSTALL_PREFIX}/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory ${CMAKE_INSTALL_PREFIX}/rkcommon/lib/ ${CMAKE_INSTALL_PREFIX}/lib
      )
  endif()

  option(HDSUPER_OSPRAY_DEPENDENCIES_ONLY "only build ospray dependencies" ON)
  option(HDSUPER_DOWNLOAD_OSPRAY_BINARIES "use pre-built ospray binaries" ON)
  # by default, download ospray binaries but we need to build rkcommon for dev includes
  if (HDSUPER_DOWNLOAD_OSPRAY_BINARIES)
    set(HDSUPER_OSPRAY_DEPENDENCIES_ONLY ON)
    set(OSPRAY_BINARIES_URL https://github.com/RenderKit/ospray/releases/download/v3.1.0/ospray-3.1.0.x86_64.linux.tar.gz)
    if (WIN32)
      set(OSPRAY_BINARIES_URL https://github.com/RenderKit/ospray/releases/download/v3.1.0/ospray-3.1.0.x86_64.windows.zip)
    elseif (APPLE)
      set(OSPRAY_BINARIES_URL https://github.com/RenderKit/ospray/releases/download/v3.1.0/ospray-3.1.0.x86_64.macosx.zip)
    endif()
    ExternalProject_Add (
      OSPRayBinaries
      PREFIX OSPRayBinaries
      STAMP_DIR OSPRayBinaries/stamp
      SOURCE_DIR OSPRayBinaries/src
      BINARY_DIR OSPRayBinaries
      URL ${OSPRAY_BINARIES_URL}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      BUILD_ALWAYS OFF
      INSTALL_COMMAND 
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/lib
        ${CMAKE_INSTALL_PREFIX}/lib
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/bin
        ${CMAKE_INSTALL_PREFIX}/bin
    )
  endif()

  set(EP_OSPRAY "OSPRay")
  message ("ARGS_OSPRAY_DOWNLOAD: ${ARGS_OSPRAY_DOWNLOAD}")
  message("OSPRAY_INSTALL COMMAND: ${OSPRAY_INSTALL_COMMAND}")
  ExternalProject_Add (
    ${EP_OSPRAY}
    PREFIX ${EP_OSPRAY}
    STAMP_DIR     ${EP_OSPRAY}/stamp
    SOURCE_DIR    ${EP_OSPRAY}/source
    BINARY_DIR    ${EP_OSPRAY}/build
    ${ARGS_OSPRAY_DOWNLOAD}
    GIT_SHALLOW   1
    GIT_SUBMODULES "scripts"  # hack: setting submodules to empty enables all submodules
    #GIT_SUBMODULES_RECURSE 0
    CONFIGURE_COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/${EP_OSPRAY}/source/scripts/superbuild
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}
      -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
      -DINSTALL_IN_SEPARATE_DIRECTORIES=ON
      -DDOWNLOAD_ISPC=${BUILD_OSPRAY_ISPC}
      -DDOWNLOAD_TBB=ON
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DOSPRAY_ENABLE_MODULES=ON
      -DBUILD_OSPRAY_APPS=OFF
      -DBUILD_OIDN=${HDSUPER_USE_DENOISER}
      -DBUILD_OIDN_FROM_SOURCE=OFF
      -DBUILD_EMBREE_FROM_SOURCE=OFF
      -DBUILD_DEPENDENCIES_ONLY=${HDOSPRAY_BUILD_OSPRAY_DEPENDENCIES_ONLY}
    INSTALL_COMMAND ${OSPRAY_INSTALL_COMMAND}
    DEPENDS ${EP_USD_SUPER}
  )

#external_install(OSPRay)
