#!/bin/bash
## Copyright 2019 Intel Corporation
## SPDX-License-Identifier: Apache-2.0
set -x

mkdir build
cd build

cmake \
  -D CMAKE_CXX_FLAGS="-std=c++17" \
  -D pxr_DIR=/gitlab/USD-install \
  -D ospray_DIR=/usr \
  -D embree_DIR=/usr \
  -D ospcommon_DIR=/usr \
  -D openvkl_DIR=/usr \
  -D PXR_BUILD_OPENIMAGEIO_PLUGIN=ON \
  -D OIIO_BASE_DIR=/gitlab/USD-install \
  -D HDOSPRAY_ENABLE_DENOISER=OFF \
  -D CMAKE_INSTALL_PREFIX=/gitlab/USD-install \
  -D GLEW_LIBRARY=/gitlab/USD-install/lib64/libGLEW.so \
  -D GLEW_INCLUDE_DIR=/gitlab/USD-install/include \
  -D PXR_ENABLE_PTEX_SUPPORT=OFF \
  -D PXR_BUILD_USD_IMAGING=OFF \
  -D PXR_BUILD_USDVIEW=OFF \
  "$@" ..

make -j`nproc`
