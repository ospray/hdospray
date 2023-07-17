#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set -x
cmake --version

THREADS=`nproc`

export ROOT_DIR=$PWD
ls $PWD
export TBB_ROOT=$PWD/install
export CMAKE_CXX_FLAGS="-std=c++17"

if [ ! -d "$ROOT_DIR/install" ]
then
    mkdir -p build_deps
    cd build_deps
    cmake ../scripts/superbuild/ -DBUILD_HDOSPRAY=OFF \
        -DHDSUPER_OSPRAY_USE_EXTERNAL=ON \
        -DHDSUPER_OSPRAY_EXTERNAL_DIR=/usr/lib/cmake/ospray-2.12.0 \
        -DBUILD_OSPRAY=OFF -DHDSUPER_USD_VERSION=v23.02 -DBUILD_TIFF=ON \
        -DBUILD_PNG=ON -DBUILD_JPEG=ON -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF \
        -DCMAKE_INSTALL_PREFIX=$ROOT_DIR/install .
    cmake --build . -j ${THREADS}
else
   echo "using cached dependencies"
fi

ls $ROOT_DIR/install

cd $ROOT_DIR
mkdir -p build_release
cd build_release
cmake -L \
  -D ospray_DIR="$ROOT_DIR/install/lib/cmake/ospray-2.12.0" \
  -D pxr_DIR="$ROOT_DIR/install" \
  -D rkcommon_DIR="$ROOT_DIR/install/rkcommon/lib/cmake/rkcommon-1.11.0" \
  -D HDOSPRAY_INSTALL_DEPENDENCIES=ON \
  -D CMAKE_INSTALL_PREFIX=$ROOT_DIR/build_release \
  ..

make -j $THREADS package || exit 2