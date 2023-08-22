#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

HOUDINI_DIR=/NAS/packages/apps/usd/ubuntu22_04/hfs19.5.640

set -x
cmake --version
env

THREADS=`nproc`

export ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps/usd-23.02
mkdir -p $ROOT_DIR/../hdospray_deps
mkdir -p $DEP_DIR
ls $PWD
export TBB_ROOT=$DEP_DIR
export CMAKE_CXX_FLAGS="-std=c++17"
echo "houdini_DIR:"
ls $HOUDINI_DIR/toolkit/cmake

if [ ! -d "$DEP_DIR/install" ]
then
    echo "houdini release build requires regular build cache"
    exit 2
fi

echo "ospray install dirs: "
ls $DEP_DIR/install/ospray/lib
ls $DEP_DIR/install/ospray/bin
ls $DEP_DIR/install/ospray/include

cd $ROOT_DIR
mkdir -p build_release
cd build_release
cmake -L \
  -D ospray_DIR="$DEP_DIR/install/ospray/lib/cmake/ospray-2.12.0" \
  -D rkcommon_DIR="$DEP_DIR/install/rkcommon/lib/cmake/rkcommon-1.11.0" \
  -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON \
  -D HDOSPRAY_GENERATE_SETUP=ON \
  -D CMAKE_INSTALL_PREFIX=$ROOT_DIR/build_release \
  -D USE_HOUDINI_USD=ON \
  -D Houdini_DIR=$HOUDINI_DIR/toolkit/cmake \
  ..

make -j $THREADS package || exit 2