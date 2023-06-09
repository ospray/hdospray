option(BUILD_TBB "" ON)
option(BUILD_TIFF "" OFF)
option(BUILD_PNG "" OFF)
option(BUILD_JPEG "" OFF)
option(BUILD_GLEW "" OFF)
option(BUILD_OPENEXR "" OFF)
option(BUILD_PTEX "" OFF)
option(BUILD_BOOST "" ON)
option(ENABLE_PTEX "enable ptex support in USD" OFF)

SET(EP_USD_SUPER "USD_Super")
  ExternalProject_Add (
    ${EP_USD_SUPER}

    PREFIX        ${EP_USD_SUPER}
    DOWNLOAD_DIR  ${EP_USD_SUPER}
    STAMP_DIR     ${EP_USD_SUPER}/stamp
    SOURCE_DIR    ${EP_USD_SUPER}/source
    BINARY_DIR    ${EP_USD_SUPER}/build
    GIT_REPOSITORY ${HDSUPER_USDSUPER_URL}
    GIT_TAG       ${HDSUPER_USDSUPER_TAG}
    GIT_SHALLOW   OFF

    LIST_SEPARATOR | # Use the alternate list separator
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
      -DBUILD_ALL_DEPS=OFF
      -DUSE_PYTHON=${USE_PYTHON}
      -DUSE_PYTHON2=OFF
      -DBUILD_TBB=${BUILD_TBB}
      -DBUILD_TIFF=${BUILD_TIFF}
      -DBUILD_PNG=${BUILD_PNG}
      -DBUILD_JPEG=${BUILD_JPEG}
      -DBUILD_BOOST=${BUILD_BOOST}
      -DBUILD_GLEW=${BUILD_GLEW}
      -DBUILD_PTEX=${BUILD_PTEX}
      -DBUILD_OPENEXR=${BUILD_OPENEXR}
      -DPXR_ENABLE_PTEX_SUPPORT=${ENABLE_PTEX}
      -DBUILD_OPENSUBDIV=ON
      -DBUILD_HDF5=OFF
      -DBUILD_OPENIMAGEIO=OFF
      -DPXR_BUILD_OPENIMAGEIO_PLUGIN=OFF
      -DUSD_VERSION=${HDSUPER_USD_VERSION}
    INSTALL_COMMAND ""
    BUILD_ALWAYS OFF
    DEPENDS ${USD_DEPENDENCIES}
  )

#  ExternalProject_Add_StepDependencies(${EP_USD_SUPER}
#        configure ${USD_DEPENDENCIES}
#      )

#ExternalProject_Get_property(USD BINARY_DIR)
#set(binDir ${BINARY_DIR})
#set(projectInfo ${binDir} "USD" "ALL" "/")
#set(depProjectInfo ${depProjectInfo} ${projectInfo})
external_install(USD_Super)