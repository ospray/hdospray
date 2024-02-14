## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

pip3.8 install PyOpenGL
pip3.8 install PySide2
pip3.8 install numpy
pip3.8 show PyOpenGL

$NAS = "*removed*"
$NAS_DEP_DIR = "$NAS\packages\apps\usd\win10"
$ROOT_DIR = pwd
$DEP_DIR = "$ROOT_DIR/../../hdospray_deps"
md -Force $DEP_DIR

#### Build HdOSPRay ####

cd $ROOT_DIR
$HOUDINI_DIR = "$DEP_DIR\houdini-19.5.682"
if (Test-Path -Path $HOUDINI_DIR ) {
} else {
  cp -r $NAS\packages\apps\usd\win10\houdini-19.5.682 $HOUDINI_DIR
}
echo "houdini_dir: "
ls $HOUDINI_DIR

#
# build rkcommon using superbuild
#
md -Force $DEP_DIR/install
if (test-path build_deps) {
  rm -r build_deps
}
md -Force build_deps
cd build_deps
cmake ../scripts/superbuild/ -DBUILD_HDOSPRAY=OFF `
    -DHDSUPER_OSPRAY_USE_EXTERNAL=ON `
    -DHDSUPER_OSPRAY_EXTERNAL_DIR="/NAS/packages/apps/usd/win10/ospray-3.0.0/lib/cmake/ospray-3.0.0" `
    -DBUILD_OSPRAY=ON -DHDSUPER_USD_VERSION=v23.08 -DBUILD_TIFF=OFF `
    -DBUILD_USD=OFF `
    -DBUILD_PNG=OFF -DBUILD_JPEG=OFF -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF `
    -DCMAKE_INSTALL_PREFIX="$DEP_DIR/install"
cmake --build . --config release -j 32
echo "dep dir install:"
ls $DEP_DIR/install

cd $ROOT_DIR
md build_release
cd build_release

echo "build_release dir:"
pwd
echo "ls build_release"
dir

$env:NAS_DEP_DIR = $NAS_DEP_DIR
$env:HOUDINI_DIR = $HOUDINI_DIR

#cmake --debug-output --trace-expand -L `
cmake -L `
  -D ospray_DIR="$NAS_DEP_DIR\ospray-3.0.0\lib\cmake\ospray-3.0.0" `
  -D Houdini_DIR="$HOUDINI_DIR" `
  -D rkcommon_DIR="$DEP_DIR\install\rkcommon\lib\cmake\rkcommon-1.12.0" `
  -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON `
  -D HDOSPRAY_INSTALL_PYTHON_DEPENDENCIES=OFF `
  -D HDOSPRAY_GENERATE_SETUP=OFF `
  -D HDOSPRAY_SIGN_FILE=$env:SIGN_FILE_WINDOWS `
  -D USE_HOUDINI_USD=ON `
  -D CMAKE_INSTALL_PREFIX=$ROOT_DIR/build_release `
  ..

cmake --build . --config release -j 32
cmake --build . --config release -j 32 --target PACKAGE
cmake -L -D HDOSPRAY_ZIP_MODE=ON .
cmake --build . --config release -j 32 --target PACKAGE

exit $LASTEXITCODE
