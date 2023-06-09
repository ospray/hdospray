#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set -x
cmake --version

THREADS=`nproc`

mkdir -p build_release
cd build_release
ls $PWD
export TBB_ROOT=$PWD/install
export CMAKE_CXX_FLAGS="-std=c++17"
cmake ../scripts/superbuild/ -DHDSUPER_OSPRAY_USE_EXTERNAL=ON -DHDSUPER_OSPRAY_EXTERNAL_DIR=/usr/lib/cmake/ospray-2.11.0 -DBUILD_OSPRAY=OFF -DHDSUPER_USD_VERSION=v23.02 -DBUILD_TIFF=ON -DBUILD_PNG=ON -DBUILD_JPEG=ON -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF .
cmake --build . -j ${THREADS}

make -j $THREADS package || exit 2