#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

#
# builders assume ospray + houdini binaries are externally
#   available in NAS_DEP_DIR
#


set -x
cmake --version

THREADS=`nproc`

echo "pip packages: "
ls /usr/local/lib/python3.8/dist-packages
pip3 install PyOpenGL==3.1.5 PySide2 numpy
echo "pip3 show PyOpenGL:"
pip3 show PyOpenGL
export ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps/usd-23.08
mkdir -p $ROOT_DIR/../hdospray_deps
mkdir -p $DEP_DIR
ls $PWD
export CMAKE_CXX_FLAGS="-std=c++17"

rm -r $DEP_DIR/install
if [ ! -d "$DEP_DIR/install" ]
then
    mkdir -p $DEP_DIR/install
    mkdir -p build_deps
    cd build_deps
    cmake ../scripts/superbuild/ -DBUILD_HDOSPRAY=OFF \
        -DHDSUPER_OSPRAY_USE_EXTERNAL=OFF \
        -DBUILD_OSPRAY=ON -DHDSUPER_USD_VERSION=v23.08 -DBUILD_TIFF=ON \
        -DBUILD_PNG=ON -DBUILD_JPEG=ON -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF \
        -DCMAKE_INSTALL_PREFIX=$DEP_DIR/install .
    cmake --build . -j ${THREADS}
else
   echo "using cached dependencies"
fi

export TBB_ROOT=$DEP_DIR/install/tbb

ls $DEP_DIR

cd $ROOT_DIR
mkdir -p build_release
cd build_release
cmake -L \
  -D ospray_DIR="$DEP_DIR/install/lib/cmake/ospray-3.1.0" \
  -D pxr_DIR="$DEP_DIR/install" \
  -D rkcommon_DIR="$DEP_DIR/install/rkcommon/lib/cmake/rkcommon-1.13.0" \
  -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON \
  -D HDOSPRAY_GENERATE_SETUP=ON \
  -D HDOSPRAY_PYTHON_PACKAGES_DIR=/usr/local/lib/python3.8/dist-packages \
  -D CMAKE_INSTALL_PREFIX=$ROOT_DIR/build_release \
  ..

make -j $THREADS package || exit 2
