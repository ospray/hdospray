##
## OpenVDB
##

set(OpenVDB_ARGS
    -DUSE_PKGCONFIG=OFF
    -DOPENVDB_BUILD_DOCS=OFF
    -DOPENVDB_BUILD_PYTHON_MODULE=OFF
    -DOPENVDB_BUILD_UNITTESTS=OFF
    -DCONCURRENT_MALLOC=None
)

set(OpenVDB_DEPENDENCIES "")

if(BUILD_TBB)
    set(OpenVDB_ARGS
        ${OpenVDB_ARGS}
        -DTBB_ROOT=${CMAKE_INSTALL_PREFIX}
    )
    set(OpenVDB_DEPENDENCIES
        ${OpenVDB_DEPENDENCIES}
        tbb
    )
endif()

if(BUILD_BOOST)
    set(OpenVDB_DEPENDENCIES
        ${OpenVDB_DEPENDENCIES}
        boost
    )
    set(OpenVDB_ARGS
        ${OpenVDB_ARGS}
        -DBOOST_ROOT=${CMAKE_INSTALL_PREFIX}
        -DBoost_NO_SYSTEM_PATHS=ON
        -DBoost_NO_BOOST_CMAKE=ON
    )
endif()

if(BUILD_OPENEXR)
    set(OpenVDB_ARGS
        ${OpenVDB_ARGS}
        -DIlmBase_ROOT=${CMAKE_INSTALL_PREFIX}
    )
    set(OpenVDB_DEPENDENCIES
        ${OpenVDB_DEPENDENCIES}
        OpenEXR
    )
endif()

build_component(
  NAME OpenVDB
  VERSION "v8.0.0"
  URL https://github.com/AcademySoftwareFoundation/openvdb
  BUILD_ARGS ${OpenVDB_ARGS}
  DEPENDS_ON ${OpenVDB_DEPENDENCIES}
)