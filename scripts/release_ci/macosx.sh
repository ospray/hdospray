#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set -x
cmake --version

#### Set variables for script ####

ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps/usd-23.02
THREADS=`sysctl -n hw.logicalcpu`
#USD_ROOT=$STORAGE_PATH/packages/apps/usd/macos/build-hdospray-superbuild
USD_ROOT=$DEP_DIR/install

echo $PWD

# set compiler if the user hasn't explicitly set CC and CXX
if [ -z $CC ]; then
  echo "***NOTE: Defaulting to use clang!"
  echo -n "         Please set env variables 'CC' and 'CXX' to"
  echo " a different supported compiler (gcc/icc) if desired."
  export CC=clang
  export CXX=clang++
fi

mkdir -p $DEP_DIR
cd $DEP_DIR
export PATH=$PATH:$DEP_DIR/install/bin

pip3.9 install PySide6
pip3.9 install PySide6-Addons
pip3.9 install PySide6-Essentials
echo "which pyside6-uic:"
which pyside6-uic
which uic
pip3.9 show PySide6

# rebuild dependencies - clear install dir
# rm -r $DEP_DIR/install

#### Build dependencies ####
if [ ! -d "$DEP_DIR/install" ]
  then
  echo "building dependencies"
  mkdir -p $USD_ROOT/install/bin
  export Python_ROOT_DIR="/usr/local/Frameworks/Python.framework/Versions/3.9"
  export Python3_ROOT_DIR="/usr/local/Frameworks/Python.framework/Versions/3.9"
  alias python=/usr/local/bin/python3.9
  alias python3=/usr/local/bin/python3.9
  export MACOSX_DEPLOYMENT_TARGET=11.7
  cmake $ROOT_DIR/scripts/superbuild/ -DHDSUPER_PYTHON_VERSION=3.9 \
    -DHDSUPER_PYTHON_EXECUTABLE=/usr/local/bin/python3.9 -DBUILD_OSPRAY=ON \
    -DBUILD_OSPRAY_ISPC=ON -DBUILD_HDOSPRAY_ISPC=OFF -DBUILD_HDOSPRAY=OFF \
    -DBUILD_USD=ON -DHDSUPER_USD_VERSION=v23.08 -DBUILD_TIFF=OFF -DBUILD_PNG=OFF \
    -DBUILD_BOOST=ON -DPYSIDE_BIN_DIR=/Users/github-runner/Library/Python/3.9/lib/python/site-packages/PySide6/Qt/libexec \
    -DBUILD_JPEG=OFF -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF .
  cmake --build . -j ${THREADS} || exit 2

  echo "install/lib:"
  ls install/lib
else
   echo "using cached dependencies"
fi

cd $ROOT_DIR

#### Build HdOSPRay ####

mkdir -p build_release
cd build_release
 Clean out build directory to be sure we are doing a fresh build
rm -rf *
cmake .. -Dpxr_DIR=$USD_ROOT -Dospray_DIR=$USD_ROOT/ospray/lib/cmake/ospray-2.12.0 \
         -Drkcommon_DIR=$USD_ROOT/rkcommon/lib/cmake/rkcommon-1.11.0 \
         -DOpenImageDenoise_DIR=$USD_ROOT/oidn/lib/cmake/OpenImageDenoise-1.4.3 \
         -DTBB_DIR=$USD_ROOT/tbb/lib/cmake/tbb -DCMAKE_BUILD_TYPE=Release \
         -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON \
         -D HDOSPRAY_GENERATE_SETUP=ON \
         -D HDOSPRAY_PYTHON_INSTALL_DIR=~/Library/Python/3.9 \
         -DHDOSPRAY_SIGN_FILE=$SIGN_FILE_MAC || exit 2
cmake --build . -j ${THREADS} || exit 2

# set release and installer settings
# create installers
make -j $THREADS package || exit 2
cpack -G ZIP || exit 2
