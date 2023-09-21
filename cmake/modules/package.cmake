## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set(OSPRAY_ROOT ${ospray_DIR}/../../../)

# packaging dependencies
option(HDOSPRAY_INSTALL_USD_DEPENDENCIES "copy usd files to install dir" OFF)
option(HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES "copy ospray files to install dir" OFF)
option(HDOSPRAY_INSTALL_PYTHON_DEPENDENCIES "copy python to install dir" OFF)
if (HDOSPRAY_INSTALL_PYTHON_DEPENDENCIES)
  #find_package(PythonLibs)
  if (${PYTHON_VERSION})
    find_package(Python ${PYTHON_VERSION} EXACT REQUIRED)
  else()
    find_package(Python 3.0 REQUIRED)
  endif()
  message("Python_LIBRARIES: ${Python_LIBRARIES}")
  message("Python3_LIBRARIES: ${Python3_LIBRARIES}")
  message("Python_EXECUTABLE: ${Python_EXECUTABLE}")
  message("Python_EXECUTABLE: ${Python3_EXECUTABLE}")
  message("Python_LIBRARY_DIRS: ${Python_LIBRARY_DIRS}")
  message("Python_RUNTIME_LIBRARY_DIRS: ${Python_RUNTIME_LIBRARY_DIRS}")
  set(PythonLib ${Python_LIBRARIES})
  if(NOT PythonLib)
      set(PythonLib ${Python3_LIBRARIES})
  endif()
  message("PythonLib: ${PythonLib}")
  get_filename_component(PythonLibDir ${PythonLib} DIRECTORY)
  get_filename_component(PythonBinDir ${Python_EXECUTABLE} DIRECTORY)
  if (WIN32)
      file(GLOB pythonLibs ${PythonLibDir}/python*)
  else()
      file(GLOB pythonLibs ${PythonLibDir}/libpython*)
  endif()
  file(GLOB pythonBins ${PythonBinDir}/python*)
  message("installing python libs: ${pythonLibs}")
  message("found python executable: \"${Python_EXECUTABLE}\"")
  message("found python executable2: \"${PYTHON_EXECUTABLE}\"")
  message("python runtime: \"${Python_RUNTIME_LIBRARY_DIRS}\"")
  message("python include: \"${Python_INCLUDE_DIRS}\"")
  message("found python bins: ${pythonBins}")
  if (HDOSPRAY_PYTHON_PACKAGES_DIR) # pip packages directory
      message("python pip packages: ${HDOSPRAY_PYTHON_PACKAGES_DIR}")
      install(DIRECTORY ${HDOSPRAY_PYTHON_PACKAGES_DIR}/
              USE_SOURCE_PERMISSIONS
              DESTINATION ./python/site-packages
      )
  endif()
  if (HDOSPRAY_PYTHON_INSTALL_DIR)
      message("installing python directory: ${HDOSPRAY_PYTHON_INSTALL_DIR}")
      install(DIRECTORY ${HDOSPRAY_PYTHON_INSTALL_DIR}/
              USE_SOURCE_PERMISSIONS
              DESTINATION ./python
      )
  else()
    install(FILES ${pythonLibs}
          DESTINATION ./python/lib)
    install(FILES ${pythonBins}
          DESTINATION ./python/bin)
  endif()
endif()
if (HDOSPRAY_INSTALL_USD_DEPENDENCIES)
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
if (HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES)
  message("installing ospray_root: ${OSPRAY_ROOT}")
    install(DIRECTORY ${OSPRAY_ROOT}/include
            USE_SOURCE_PERMISSIONS
            DESTINATION .
    )
    install(DIRECTORY ${OSPRAY_ROOT}/lib
            USE_SOURCE_PERMISSIONS
            DESTINATION .
    )
    if (EXISTS ${OSPRAY_ROOT}/bin)
        install(DIRECTORY ${OSPRAY_ROOT}/bin
                USE_SOURCE_PERMISSIONS
                DESTINATION .
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
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OSPRay for Hydra is a RenderDelegate plugin for USD")
set(CPACK_PACKAGE_VENDOR "Intel Corporation")
set(CPACK_PACKAGE_CONTACT https://github.com/ospray/hdospray)

# point to readme and license files
set(CPACK_RESOURCE_FILE_README ${PROJECT_SOURCE_DIR}/README.md)
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.txt)

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
    math(EXPR OSPRAY_VERSION_NUMBER "10000*${HDOSPRAY_VERSION_MAJOR} + 100*${HDOSPRAY_VERSION_MINOR} + ${HDOSPRAY_VERSION_PATCH}")
    #set(CPACK_WIX_PRODUCT_GUID "7dfff8de-2c61-4f49-b46b-d837a53e6646${HDOSPRAY_VERSION_NUMBER}")
    #set(CPACK_WIX_UPGRADE_GUID "7dfff8de-2c61-4f49-b46b-d837a53e6646${HDOSPRAY_VERSION_MAJOR}0000") # upgrade as long as major version is the same
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
  if (WIN32)
    FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.ps1
        "\$env:PYTHONPATH += \";\$PSScriptRoot\\lib\\python\"\n"
        "\$env:PYTHONPATH += \";\$PSScriptRoot\\python\\lib\"\n"
        "\$env:PYTHONPATH += \";\$PSScriptRoot\\python\\lib\\site-packages\"\n"
        "\$env:Path += \";\$PSScriptRoot\\bin\"\n"
        "\$env:Path += \";\$PSScriptRoot\\ospray\\bin\"\n"
        "\$env:Path += \";\$PSScriptRoot\\plugin\\usd\"\n"
        "\$env:Path += \";\$PSScriptRoot\\lib\"\n"
        "\$env:Path = \"\$PSScriptRoot\\python;\" + \$env:Path\n"
        "Start-Process .\\bin\\usdview -NoNewWindow -ArgumentList \$args[0]"
        )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.ps1
            DESTINATION .)
  else()
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
        if (HDOSPRAY_INSTALL_USD_DEPENDENCIES)
          FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/run_usdview.sh
              "#!/bin/bash\n"
              "export HDOSPRAY_ROOT=$( cd -- \"\$( dirname -- \"\${BASH_SOURCE[0]}\" )\" && pwd )\n"
              "source \${HDOSPRAY_ROOT}/setup_hdospray.sh\n"
              "python \${HDOSPRAY_ROOT}/bin/usdview \$*"
              )
          #message("installing into: ${CMAKE_CURRENT_BINARY_DIR}")
          install(FILES ${CMAKE_CURRENT_BINARY_DIR}/run_usdview.sh
              DESTINATION .
              PERMISSIONS OWNER_EXECUTE OWNER_READ
                          GROUP_EXECUTE GROUP_READ
              )
        endif()
        FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.sh
            "#!/bin/bash\n"
            "export HDOSPRAY_ROOT=$( cd -- \"\$( dirname -- \"\${BASH_SOURCE[0]}\" )\" && pwd )\n"
            "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/lib:\${${LD_EXPORT}}\n"
            "export ${LD_EXPORT}=\${HDOSPRAY_ROOT}/python/lib:\${${LD_EXPORT}}\n"
            "export PYTHONPATH=\${HDOSPRAY_ROOT}/lib/python:\${PYTHONPATH}\n"
            "export PYTHONPATH=\${HDOSPRAY_ROOT}/python/lib:\${PYTHONPATH}\n"
            "export PYTHONPATH=\${HDOSPRAY_ROOT}/python/site-packages:\${PYTHONPATH}\n"
            "export PATH=\${HDOSPRAY_ROOT}/bin:\${PATH}\n"
            "export PATH=\${HDOSPRAY_ROOT}/python/bin:\${PATH}\n"
            "export PXR_PLUGINPATH_NAME=\${HDOSPRAY_ROOT}/plugin/usd/hdOSPRay/resources:\${PXR_PLUGINPATH_NAME}\n"
            )
    endif()
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/setup_hdospray.sh
            DESTINATION .)
  endif()
endif()
