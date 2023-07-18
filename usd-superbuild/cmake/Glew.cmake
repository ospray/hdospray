##
## GLEW
##

set (EP_GLEW "glew")

set(GLEW_ARGS ""
)

set(GLEW_DEPENDENCIES ""
#  boost #only needed if python libs are build
)

set(GLEW_URL "https://downloads.sourceforge.net/project/glew/glew/2.0.0/glew-2.0.0.tgz")
if (WIN32)
	set(GLEW_URL "https://downloads.sourceforge.net/project/glew/glew/2.0.0/glew-2.0.0-win32.zip")
endif()

ExternalProject_Add (
  ${EP_GLEW}
  PREFIX ${EP_GLEW}
  BINARY_DIR ${EP_GLEW}/build
  SOURCE_DIR ${EP_GLEW}/source
  #CONFIGURE_COMMAND "cmake ../source/build/cmake -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ."
  URL ${GLEW_URL}
  BUILD_ALWAYS OFF
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ../source/build/cmake
)