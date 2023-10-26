## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

pip3.8 install PyOpenGL
pip3.8 install PySide2
pip3.8 install numpy
pip3.8 show PyOpenGL

$NAS = "*removed*"
$NAS_DEP_DIR = "$NAS\packages\apps\usd\win10"
$ROOT_DIR = pwd
$DEP_DIR = "../hdospray_deps/usd-23.02"
md $DEP_DIR

## Build dependencies ##
# Windows CI dependencies are prebuilt

cd $ROOT_DIR

#### Build HdOSPRay ####

md build_release
cd build_release

# Clean out build directory to be sure we are doing a fresh build
#rm -r -fo *

cmake -L `
  -D ospray_DIR="$NAS_DEP_DIR\ospray-2.12.0.x86_64.windows\lib\cmake\ospray-2.12.0" `
  -D pxr_DIR="$NAS_DEP_DIR\usd-23.02" `
  -D rkcommon_DIR="$NAS_DEP_DIR\rkcommon\lib\cmake\rkcommon-1.11.0" `
  -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON `
  -D HDOSPRAY_GENERATE_SETUP=ON `
  -D HDOSPRAY_PYTHON_INSTALL_DIR="C:\Program Files\Python38" `
  -D HDOSPRAY_SIGN_FILE=$env:SIGN_FILE_WINDOWS `
  -D PYTHON_VERSION=3.8 `
  ..

cmake --build . --config release -j 32
cmake --build . --config release -j 32 --target PACKAGE
cpack -G ZIP

exit $LASTEXITCODE