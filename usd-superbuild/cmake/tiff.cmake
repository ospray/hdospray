##
## libtiff
##

set (EP_TIFF "tiff")

set(TIFF_ARGS ""
)

set(TIFF_DEPENDENCIES ""
#  boost #only needed if python libs are build
)

ExternalProject_Add (
  ${EP_TIFF}
  PREFIX ${EP_TIFF}
  BINARY_DIR ${EP_TIFF}/build
  SOURCE_DIR ${EP_TIFF}/source
  URL "https://download.osgeo.org/libtiff/tiff-4.0.7.zip"
  BUILD_ALWAYS OFF
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
  #INSTALL_COMMAND ""
)