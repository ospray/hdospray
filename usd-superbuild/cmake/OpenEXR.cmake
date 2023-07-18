##
## OpenEXR
##

set(OpenEXR_ARGS ""
  -DPYILMBASE_ENABLE=OFF
  -DOPENEXR_VIEWERS_ENABLE=OFF
  #-DBOOST_ROOT=${CMAKE_INSTALL_PREFIX}  #only needed if python libs are build
)

set(OpenEXR_DEPENDENCIES ""
#  boost #only needed if python libs are build
)

# OpenEXR is adding "_d" to library names on windows.  Fun.
if (WIN32)
  set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES};_d.lib")
endif()

build_component(
  NAME OpenEXR
  VERSION "v2.4.0"
  URL "https://github.com/AcademySoftwareFoundation/openexr"
  BUILD_ARGS ${OpenEXR_ARGS}
  DEPENDS_ON ${OpenEXR_DEPENDENCIES}
)