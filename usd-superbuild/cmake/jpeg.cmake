##
## libtiff
##

set (EP_JPEG "jpeg")

set(JPEG_ARGS "")

set(JPEG_URL "https://www.ijg.org/files/jpegsrc.v9b.tar.gz")
if (WIN32)
   set(JPEG_URL "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.1.zip")
endif()


set(JPEG_DEPENDENCIES ""
#  boost #only needed if python libs are build
)

if (WIN32)
  ExternalProject_Add (
    ${EP_JPEG}
    PREFIX ${EP_JPEG}
    BINARY_DIR ${EP_JPEG}/build
    SOURCE_DIR ${EP_JPEG}/source
    URL ${JPEG_URL}
    BUILD_ALWAYS OFF
    INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    #CONFIGURE_COMMAND ../source/configure --prefix=${CMAKE_INSTALL_PREFIX}
    #BUILD_COMMAND make -j8
    #INSTALL_COMMAND make install
  )
else()
  ExternalProject_Add (
    ${EP_JPEG}
    PREFIX ${EP_JPEG}
    BINARY_DIR ${EP_JPEG}/build
    SOURCE_DIR ${EP_JPEG}/source
    URL ${JPEG_URL}
    BUILD_ALWAYS OFF
    INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    CONFIGURE_COMMAND ../source/configure --prefix=${CMAKE_INSTALL_PREFIX}
    BUILD_COMMAND make -j8
    INSTALL_COMMAND make install
  )
endif()

#add_library(${EP_JPEG} STATIC IMPORTED)
#set_property(TARGET ${EP_JPEG} PROPERTY IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/lib/libjpeg.a)
#set_property(TARGET ${EP_JPEG} PROPERTY INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
#add_dependencies(${EP_JPEG} project_${EP_JPEG})
#target_link_libraries(USD_superbuild ${EP_JPEG})
#set(projectInfo ${CMAKE_INSTALL_PREFIX} "jpeg" "ALL" "/")
#set(depProjectInfo ${depProjectInfo} ${projectInfo})
#external_install(jpeg)