#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

HOUDINI_DIR=/NAS/packages/apps/usd/ubuntu22_04/hfs19.5.640

set -x
cmake --version
env

THREADS=`nproc`

apt install python3-pip -y
echo "pip packages: "
ls /usr/local/lib/python3.8/dist-packages
pip3 install PyOpenGL==3.1.5 PySide2 numpy
echo "pip3 show PyOpenGL:"
pip3 show PyOpenGL

export ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps/usd-23.02
mkdir -p $ROOT_DIR/../hdospray_deps
mkdir -p $DEP_DIR
ls $PWD
export TBB_ROOT=$DEP_DIR
export CMAKE_CXX_FLAGS="-std=c++17"
echo "houdini_DIR:"
ls $HOUDINI_DIR/toolkit/cmake

rm -r $DEP_DIR/install
if [ ! -d "$DEP_DIR/install" ]
then
    mkdir -p $DEP_DIR/install
    rm -rf build_deps
    mkdir -p build_deps
    cd build_deps
    cmake ../scripts/superbuild/ -DBUILD_HDOSPRAY=OFF \
        -DHDSUPER_OSPRAY_USE_EXTERNAL=ON \
        -DHDSUPER_OSPRAY_EXTERNAL_DIR="/NAS/packages/apps/usd/ubuntu22_04/ospray-3.0.0/lib/cmake/ospray-3.0.0" \
        -DBUILD_OSPRAY=ON -DHDSUPER_USD_VERSION=v23.08 -DBUILD_TIFF=ON \
        -DBUILD_USD=OFF \
        -DBUILD_PNG=ON -DBUILD_JPEG=ON -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF \
        -DCMAKE_INSTALL_PREFIX=$DEP_DIR/install .
    cmake --build . -j ${THREADS}
else
   echo "using cached dependencies"
fi

echo "ospray install dirs: "
ls $DEP_DIR/install/ospray/lib
ls $DEP_DIR/install/ospray/bin
ls $DEP_DIR/install/ospray/include

cd $ROOT_DIR
mkdir -p build_release
cd build_release
echo "rkcommon_DIR: "
echo "$DEP_DIR/install/rkcommon/lib/cmake/rkcommon-1.12.0"
cmake -L \
  -D ospray_DIR="/NAS/packages/apps/usd/ubuntu22_04/ospray-3.0.0/lib/cmake/ospray-3.0.0" \
  -D rkcommon_DIR="$DEP_DIR/install/rkcommon/lib/cmake/rkcommon-1.12.0" \
  -D TBB_DIR="/NAS/packages/apps/usd/ubuntu22_04/ospray-3.0.0/lib/cmake/tbb" \
  -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON \
  -D HDOSPRAY_INSTALL_USD_DEPENDENCIES=OFF \
  -D HDOSPRAY_GENERATE_SETUP=ON \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_INSTALL_PREFIX=$ROOT_DIR/build_release \
  -D USE_HOUDINI_USD=ON \
  -D Houdini_DIR=$HOUDINI_DIR/toolkit/cmake \
  ..

#make -j $THREADS package || exit 2
