##
##  libzlib
##

set (EP_ZLIB "zlib")

set(ZLIB_ARGS ""
)

set(ZLIB_DEPENDENCIES ""
#  boost #only needed if python libs are build
)

ExternalProject_Add (
  ${EP_ZLIB}
  PREFIX ${EP_ZLIB}
  BINARY_DIR ${EP_ZLIB}/build
  SOURCE_DIR ${EP_ZLIB}/source
  URL "https://github.com/madler/zlib/archive/v1.2.11.zip"
  BUILD_ALWAYS OFF
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
)