#option(USE_PYTHON "enable python support" FALSE)
#option(USE_PYTHON2 "if USE_PYTHON enabled, use python2 instead of python3" FALSE)

set (EP_BOOST "boost")

set(BOOST_BOOTSTRAP "./bootstrap.sh")
set(BOOST_B2 "./b2")
if (WIN32)
  set(BOOST_BOOTSTRAP "bootstrap.bat")
  set(BOOST_B2 "b2")
endif()

set(BOOST_CONFIGURE_COMMAND ${BOOST_BOOTSTRAP} --prefix=${CMAKE_INSTALL_PREFIX})
set(BOOST_BUILD_COMMAND "${BOOST_B2} install --prefix=${CMAKE_INSTALL_PREFIX} --build-dir=${CMAKE_CURRENT_BINARY_DIR}/build -j${BUILD_JOBS} address-model=64 link=shared runtime-link=shared threading=multi variant=release --with-atomic --with-program_options --with-regex --with-date_time --with-system --with-thread --with-iostreams --with-filesystem --with-serialization --with-wave --with-chrono")
# --layout=system removes named include directory, but also changes names of libraries which breaks some dependencies

set(BOOST_Python_VERSIONS)
#set(Python_ROOT_DIR /usr/local/Frameworks/Python.framework/Versions/3.9)

message("boost.cmake")
if (USE_PYTHON)
  message("writing file: " ${CMAKE_CURRENT_BINARY_DIR}/source/boost/python-config.jam)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/source/boost/python-config.jam)
  if (USE_PYTHON2)
    find_package(Python 2.7 REQUIRED)
  else()
    if (${PYTHON_VERSION})
      find_package(Python ${PYTHON_VERSION} EXACT REQUIRED)
    else()
      find_package(Python 3.0 REQUIRED)
    endif()
  endif()
  if (${PYTHON_EXECUTABLE})
    set(${PYTHON_EXECUTABLE ${Python_EXECUTABLE}})
  endif()
  message("boost.cmake python executable: " ${PYTHON_EXECUTABLE})
  message("boost.cmake python version found: ${Python_VERSION_MAJOR} ${Python_VERSION_MINOR}")
  file(
    APPEND ${CMAKE_CURRENT_BINARY_DIR}/source/boost/python-config.jam
    "using python : ${Python_VERSION_MAJOR}.${Python_VERSION_MINOR} : ${PYTHON_EXECUTABLE} ; \n"
  )
  list(APPEND BOOST_Python_VERSIONS "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
  list(JOIN BOOST_Python_VERSIONS "," BOOST_Python_VERSIONS)

  set(BOOST_BUILD_COMMAND "${BOOST_BUILD_COMMAND}  --user-config=${CMAKE_CURRENT_BINARY_DIR}/source/boost/python-config.jam python=${BOOST_Python_VERSIONS} --with-python")
endif()

if(WIN32)
  set(BOOST_URL "https://downloads.sourceforge.net/project/boost/boost/1.70.0/boost_1_70_0.tar.gz")
  set(USD_ARGS ${USD_ARGS} "-DBoost_INCLUDE_DIR=${CMAKE_INSTALL_PREFIX}/include/boost-1_70")
  if (${MSVC_VERSION} GREATER_EQUAL "142")
    set(BOOST_BUILD_COMMAND "${BOOST_BUILD_COMMAND} toolset=msvc-14.2 ")
  elseif (${MSVC_VERSION} EQUAL "142")
    set(BOOST_BUILD_COMMAND ${BOOST_BUILD_COMMAND} toolset=msvc-14.2)
  elseif (${MSVC_VERSION} EQUAL "141")
    set(BOOST_BUILD_COMMAND ${BOOST_BUILD_COMMAND} toolset=msvc-14.1)
  else()
    set(BOOST_BUILD_COMMAND ${BOOST_BUILD_COMMAND} toolset=msvc-14.0)
  endif()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/build_boost.bat ${BOOST_BUILD_COMMAND})
elseif (APPLE)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/build_boost.sh ${BOOST_BUILD_COMMAND})
  set(BOOST_BUILD_COMMAND ${BOOST_BUILD_COMMAND} toolset=clang)
  set(BOOST_URL https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_79_0.tar.gz)
else()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/build_boost.sh ${BOOST_BUILD_COMMAND})
  set(BOOST_URL "https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz" CACHE STRING "Boost URL")
endif()

# BUILD_COMMAND does not like strings, so we run scripts
if (WIN32)
  ExternalProject_Add (
    ${EP_BOOST}
    PREFIX         ${EP_BOOST}/source/boost
    BUILD_IN_SOURCE 1
    URL ${BOOST_URL}
    BUILD_ALWAYS   OFF
    LIST_SEPARATOR | # Use the alternate list separator
    CONFIGURE_COMMAND ${BOOST_CONFIGURE_COMMAND}
    BUILD_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/build_boost.bat
    INSTALL_COMMAND ""
    INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  )
else()
  ExternalProject_Add (
    ${EP_BOOST}
    PREFIX         ${EP_BOOST}/source/boost
    BUILD_IN_SOURCE 1
    URL ${BOOST_URL}
    BUILD_ALWAYS   OFF
    LIST_SEPARATOR | # Use the alternate list separator
    CONFIGURE_COMMAND ${BOOST_CONFIGURE_COMMAND}
    BUILD_COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/build_boost.sh
    INSTALL_COMMAND ""
    INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  )
endif()