##
##  libpng
##

set (EP_PNG "png")

set(PNG_ARGS ""
)

set(PNG_DEPENDENCIES ${EP_ZLIB})

ExternalProject_Add (
  ${EP_PNG}
  PREFIX ${EP_PNG}
  BINARY_DIR ${EP_PNG}/build
  SOURCE_DIR ${EP_PNG}/source
  URL "https://downloads.sourceforge.net/project/libpng/libpng16/older-releases/1.6.29/libpng-1.6.29.tar.gz"
  BUILD_ALWAYS OFF
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
  DEPENDS_ON ${PNG_DEPENDENCIES}
)