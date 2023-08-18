##
## OCIO
##

if (WIN32)
    string(REGEX REPLACE "/W[1-4]" "/W0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(OCIO_ARGS "-DCMAKE_CXX_FLAGS:STRING=/W0")
    set(OCIO_ARGS "-DCMAKE_CXX_FLAGS_RELEASE:STRING=/MD /O2 /Ob2")
    add_compile_options("/W0")
else()
    set(OCIO_ARGS "-DCMAKE_CXX_FLAGS:STRING=-fPIC -w")
    add_compile_options("-fPIC -w)"
endif()
set(OCIO_ARGS ${OCIO_ARGS}
    -DOCIO_BUILD_TRUELIGHT=OFF
    -DOCIO_BUILD_APPS=OFF
    -DOCIO_BUILD_NUKE=OFF
    -DOCIO_BUILD_DOCS=OFF
    -DOCIO_BUILD_TESTS=OFF
    -DOCIO_BUILD_PYGLUE=OFF
    -DOCIO_BUILD_JNIGLUE=OFF
    -DOCIO_STATIC_JNIGLUE=OFF
    -DOCIO_BUILD_STATIC=OFF
    -DOCIO_BUILD_SHARED=ON
    -DYAML_CPP_OBJECT_LIB_EMBEDDED=ON
    -DTINYXML_OBJECT_LIB_EMBEDDED=OFF
)

set(OCIO_DEPENDENCIES "")

if(BUILD_OPENEXR)
    set(OCIO_DEPENDENCIES
        ${OCIO_DEPENDENCIES}
        OpenEXR
    )
endif()

if(BUILD_TIFF)
    set(OCIO_DEPENDENCIES
        ${OCIO_DEPENDENCIES}
        tiff
    )
endif()

if(BUILD_PNG)
    set(OCIO_DEPENDENCIES
        ${OCIO_DEPENDENCIES}
        png
    )
endif()

if(BUILD_JPEG)
    set(OCIO_DEPENDENCIES
        ${OCIO_DEPENDENCIES}
        jpeg
    )
endif()

build_component(
 NAME OCIO
 VERSION "v1.1.0"
 URL "https://github.com/AcademySoftwareFoundation/OpenColorIO"
 BUILD_ARGS ${OCIO_ARGS}
 DEPENDS_ON ${OCIO_DEPENDENCIES}
)