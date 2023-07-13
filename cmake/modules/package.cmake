## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

# packaging dependencies
message("include install dir: ${pxr_DIR}/include")
message("cmake include install dir: ${CMAKE_INSTALL_INCLUDEDIR}")
option(HDOSPRAY_INSTALL_DEPENDENCIES "copy usd and ospray files to install dir" OFF)
if (HDOSPRAY_INSTALL_DEPENDENCIES)
    if(NOT ${pxr_DIR} MATCHES ${CMAKE_INSTALL_PREFIX})
    message("pxr_DIR: ${pxr_DIR}")
        install(DIRECTORY ${pxr_DIR}/include
                USE_SOURCE_PERMISSIONS
                DESTINATION .
        )
        install(DIRECTORY ${pxr_DIR}/lib
                USE_SOURCE_PERMISSIONS
                DESTINATION .
        )
        install(DIRECTORY ${pxr_DIR}/bin
                USE_SOURCE_PERMISSIONS
                DESTINATION .
        )
        install(DIRECTORY ${pxr_DIR}/plugin
                USE_SOURCE_PERMISSIONS
                DESTINATION .
        )
    endif()
    if(NOT ${OSPRAY_ROOT} MATCHES ${CMAKE_INSTALL_PREFIX})
        install(DIRECTORY ${OSPRAY_ROOT}/include
                USE_SOURCE_PERMISSIONS
                DESTINATION ${CMAKE_INSTALL_PREFIX}
        )
        install(DIRECTORY ${OSPRAY_ROOT}/lib
                USE_SOURCE_PERMISSIONS
                DESTINATION ${CMAKE_INSTALL_PREFIX}
        )
        install(DIRECTORY ${OSPRAY_ROOT}/bin
                USE_SOURCE_PERMISSIONS
                DESTINATION ${CMAKE_INSTALL_PREFIX}
        )
    endif()
endif()

## CPACK settings ##

set(CPACK_PACKAGE_NAME "HdOSPRay")
set(CPACK_PACKAGE_FILE_NAME "hdospray-${HDOSPRAY_VERSION}.x86_64")
if (APPLE AND HDOSPRAY_SIGN_FILE)
  # on OSX we strip files during signing
  set(CPACK_STRIP_FILES FALSE)
else()
  # do not disable, stripping symbols is important for security reasons
  set(CPACK_STRIP_FILES TRUE)
endif()

set(CPACK_PACKAGE_VERSION_MAJOR ${OSPRAY_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${OSPRAY_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${OSPRAY_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OSPRay: A Ray Tracing Based Rendering Engine for High-Fidelity Visualization")
set(CPACK_PACKAGE_VENDOR "Intel Corporation")
set(CPACK_PACKAGE_CONTACT ospray@googlegroups.com)

# point to readme and license files
set(CPACK_RESOURCE_FILE_README ${PROJECT_SOURCE_DIR}/README.md)
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.txt)

#if (HDOSPRAY_ZIP_MODE)
#  set(CPACK_MONOLITHIC_INSTALL ON)
#else()
  set(CPACK_COMPONENTS_ALL package_usd)
#endif()

if (WIN32) # Windows specific settings

  if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64bit architecture supported.")
  endif()

  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.windows")

  if (HDOSPRAY_ZIP_MODE)
    set(CPACK_GENERATOR ZIP)
  else()
    set(CPACK_GENERATOR WIX)
    set(CPACK_WIX_ROOT_FEATURE_DESCRIPTION "OSPRay for Hydra is a RenderDelegate plugin for USD.")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT https://github.com/ospray/hdospray)
    set(CPACK_PACKAGE_NAME "HdOSPRay v${HDOSPRAY_VERSION}")
    list(APPEND CPACK_COMPONENTS_ALL redist)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Intel\\\\HdOSPRay v${HDOSPRAY_VERSION_MAJOR}")
    math(EXPR OSPRAY_VERSION_NUMBER "10000*${HDOSPRAY_VERSION_MAJOR} + 100*${HDOSPRAY_VERSION_MINOR} + ${OSPRAY_VERSION_PATCH}")
    set(CPACK_WIX_PRODUCT_GUID "7dfff8de-2c61-4f49-b46b-d837a53e6646${HDOSPRAY_VERSION_NUMBER}")
    set(CPACK_WIX_UPGRADE_GUID "7dfff8de-2c61-4f49-b46b-d837a53e6646${HDOSPRAY_VERSION_MAJOR}0000") # upgrade as long as major version is the same
    set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY TRUE)
  endif()

elseif(APPLE) # MacOSX specific settings

  configure_file(${PROJECT_SOURCE_DIR}/README.md ${PROJECT_BINARY_DIR}/ReadMe.txt COPYONLY)
  set(CPACK_RESOURCE_FILE_README ${PROJECT_BINARY_DIR}/ReadMe.txt)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.macosx")

  if (HDOSPRAY_ZIP_MODE)
    set(CPACK_GENERATOR ZIP)
  else()
    set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
    set(CPACK_GENERATOR productbuild)
    set(CPACK_PACKAGE_NAME hdospray-${HDOSPRAY_VERSION})
    set(CPACK_PACKAGE_VENDOR "intel") # creates short name com.intel.ospray.xxx in pkgutil
  endif()

else() # Linux specific settings
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.linux")
  set(CPACK_GENERATOR TGZ)
endif()

option(HDOSPRAY_GENERATE_SETUP "generate setup file for hdospray" OFF)
if (HDOSPRAY_GENERATE_SETUP)
  set(LD_EXPORT "LD_LIBRARY_PATH")
  if (APPLE)
    set(LD_EXPORT "DYLD_LIBRARY_PATH")

  endif()
  if (HDSUPER_USE_HOUDINI)
    FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.sh
    "#!/bin/bash\n"
    "export HDOSPRAY_ROOT=SCRIPT_DIR=$( cd -- \"\$( dirname -- \"\${BASH_SOURCE[0]}\" )\" && pwd )\n"
    "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/lib:\${${LD_EXPORT}}\n"
    "cd ${HDSUPER_HOUDINI_DIR}/../../\n"
    "source houdini_setup_bash\n"
    "cd -\n"
    "export PXR_PLUGINPATH_NAME=\${HDOSPRAY_ROOT}/plugin/usd/hdOSPRay/resources:\${PXR_PLUGINPATHNAME}\n"
    )
  else()
    FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.sh
    "#!/bin/bash\n"
    "export HDOSPRAY_ROOT=$( cd -- \"\$( dirname -- \"\${BASH_SOURCE[0]}\" )\" && pwd )\n"
    "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/lib:\${${LD_EXPORT}}\n"
    "export PYTHONPATH=\${HDOSPRAY_ROOT}/lib/python:\${PYTHONPATH}\n"
    "export PATH=\${HDOSPRAY_ROOT}/bin:\${PATH}\n"
    "export PXR_PLUGINPATH_NAME=\${HDOSPRAY_ROOT}/plugin/usd/hdOSPRay/resources:\${PXR_PLUGINPATHNAME}\n"
    )
  endif()
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.sh
          DESTINATION .)
endif()