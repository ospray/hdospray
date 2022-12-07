#!/bin/bash -x
## Copyright 2019 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

pacman -Syu --noconfirm
pacman -S lib32-glibc --noconfirm
pacman -S libaio --noconfirm
pacman -S inetutils --noconfirm

KW_SERVER_PATH=$KW_PATH/server
KW_CLIENT_PATH=$KW_PATH/client
export KLOCWORK_LTOKEN=/tmp/ltoken

echo "$KW_SERVER_IP;$KW_SERVER_PORT;$KW_USER;$KW_LTOKEN" > $KLOCWORK_LTOKEN

mkdir -p $CI_PROJECT_DIR/klocwork
log_file=$CI_PROJECT_DIR/klocwork/build.log

mkdir build
cd build

cmake \
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

$KW_CLIENT_PATH/bin/kwinject make -j`nproc` | tee -a $log_file
$KW_SERVER_PATH/bin/kwbuildproject --classic --force --url http://$KW_SERVER_IP:$KW_SERVER_PORT/$KW_PROJECT_NAME --tables-directory $CI_PROJECT_DIR/kw_tables kwinject.out | tee -a $log_file
$KW_SERVER_PATH/bin/kwadmin --url http://$KW_SERVER_IP:$KW_SERVER_PORT/ load --force --name build-$CI_JOB_ID $KW_PROJECT_NAME $CI_PROJECT_DIR/kw_tables | tee -a $log_file

# Store kw build name for check status later
echo "build-$CI_JOB_ID" > $CI_PROJECT_DIR/klocwork/build_name

