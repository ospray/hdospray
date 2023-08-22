
find_package(Houdini CONFIG REQUIRED
    PATH_SUFFIXES toolkit/cmake)
message("Houdini_DIR: ${Houdini_DIR}")

set(Houdini_ROOT ${Houdini_DIR}/../../)
# we build hdOSPRAy against Houdinis USD

if (WIN32)
    set(Python_ROOT_DIR  ${Houdini_ROOT}/python39)
    set(Python3_ROOT_DIR  ${Houdini_ROOT}/python39)
    find_package(PythonLibs 3.9 REQUIRED)
    set(HOUDINI_CUSTOM_TARGETS ${PYTHON_LIBRARIES} ${HOUDINI_CUSTOM_TARGETS})
    message("Houdini PYTHON_LIBRARIES: ${PYTHON_LIBRARIES}")
    set (HOUDINI_IMPLIB_DIR "${Houdini_ROOT}/custom/houdini/dsolib")
    # houdini target on windows missing usd targets, so we create them
    set(HOUDINI_LIB_DIR ${Houdini_ROOT}/bin)
    set(HOUDINI_INCLUDE_DIR ${Houdini_ROOT}/toolkit/include)
    find_file(
        BOOST_PYTHON_LIB
        "hboost_python39-mt-x64.lib"
        PATHS ${Houdini_ROOT}/
        PATH_SUFFIXES
        "custom/houdini/dsolib"
    )
    foreach(usdLib
        arch
        ar
        cameraUtil
        garch
        gf
        glf
        hdMtlx
        hd
        hdSt
        hdx
        hf
        hgiGL
        hgiInterop
        hgi
        hio
        js
        kind
        ndr
        pcp
        plug
        pxOsd
        sdf
        sdr
        tf
        trace
        usdAppUtils
        #usdBakeMtlx
        usdGeom
        usdHydra
        usdImagingGL
        usdImaging
        usdLux
        usdMedia
        #usdMtlx
        usdPhysics
        usdRender
        usdRiImaging
        usdRi
        usdShade
        usdSkelImaging
        usdSkel
        usd
        usdUI
        usdUtils
        usdviewq
        usdVolImaging
        usdVol
        vt
        work
        )

        add_library(${usdLib} SHARED IMPORTED)
        set_target_properties(${usdLib} PROPERTIES
            IMPORTED_LOCATION "${HOUDINI_LIB_DIR}/libpxr_${usdLib}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES ${HOUDINI_INCLUDE_DIR})
        set_target_properties(${usdLib} PROPERTIES
            IMPORTED_IMPLIB "${HOUDINI_IMPLIB_DIR}/libpxr_${usdLib}.lib")
        set(HOUDINI_CUSTOM_TARGETS ${usdLib} ${HOUDINI_CUSTOM_TARGETS})
    endforeach()
    # linking errors without adding boost_python to some usd targets
    target_link_libraries(tf INTERFACE ${BOOST_PYTHON_LIB})
    target_link_libraries(vt INTERFACE ${BOOST_PYTHON_LIB})
elseif (APPLE)
else ()
    set(HOUDINI_LIB_DIR ${Houdini_ROOT}/dsolib)
    set(HOUDINI_INCLUDE_DIR "${Houdini_ROOT}/toolkit/include")
    add_library(pxr_hio SHARED IMPORTED)
    set_target_properties(pxr_hio PROPERTIES
        IMPORTED_LOCATION "${HOUDINI_LIB_DIR}/libpxr_hio${CMAKE_SHARED_LIBRARY_SUFFIX}"
        INTERFACE_INCLUDE_DIRECTORIES ${HOUDINI_INCLUDE_DIR})
    set(HOUDINI_CUSTOM_TARGETS pxr_hio)
endif(WIN32)