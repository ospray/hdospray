## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

pip3.8 install PyOpenGL
pip3.8 install PySide2
pip3.8 install numpy
pip3.8 show PyOpenGL

$STORAGE_DEP_DIR = "$STORAGE_PATH\packages\apps\usd\win10"
$ROOT_DIR = pwd
$DEP_DIR = "../hdospray_deps/usd-23.02"
md $DEP_DIR


cd $ROOT_DIR
## Build dependencies ##
md -Force $DEP_DIR/install
if (test-path build_deps) {
  rm -r build_deps
}
md -Force build_deps
cd build_deps
cmake ../scripts/superbuild/ -DBUILD_HDOSPRAY=OFF `
    -DHDSUPER_OSPRAY_USE_EXTERNAL=OFF `
    -DBUILD_OSPRAY=ON -DHDSUPER_USD_VERSION=v23.08 -DBUILD_TIFF=OFF `
    -DBUILD_USD=OFF `
    -DBUILD_PNG=OFF -DBUILD_JPEG=OFF -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF `
    -DCMAKE_INSTALL_PREFIX="$DEP_DIR/install"
cmake --build . --config release -j 32
echo "dep dir install:"
ls $DEP_DIR/install
echo "dep dir install ospray:"
ls $DEP_DIR/install/ospray
ls $DEP_DIR/install/ospray/lib
ls $DEP_DIR/install/ospray/lib/cmake


#### Build HdOSPRay ####
cd $ROOT_DIR
md build_release
cd build_release

# Clean out build directory to be sure we are doing a fresh build
#rm -r -fo *

cmake -L `
  -D ospray_DIR="$DEP_DIR/install/ospray/lib/cmake/ospray-3.1.0" `
  -D pxr_DIR="$STORAGE_DEP_DIR\usd-23.02" `
  -D rkcommon_DIR="$DEP_DIR\install\rkcommon\lib\cmake\rkcommon-1.13.0" `
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
