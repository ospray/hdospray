#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set -x
cmake --version

echo "NAS:"
ls /NAS
echo "/:"
ls /

echo "env:"
env

echo "deps: "
ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps
echo "deps/usd-23.02/install: "
ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install
echo "deps/usd-23.02/install/ospray: "
ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/ospray
echo "deps/usd-23.02/install/ospray/bin: "
ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/ospray/bin
echo "done"

#### Set variables for script ####

ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps
THREADS=`sysctl -n hw.logicalcpu`
#USD_ROOT=$STORAGE_PATH/packages/apps/usd/macos/build-hdospray-superbuild
USD_ROOT=$DEP_DIR/usd-23.02/install
HOUDINI_ROOT=$DEP_DIR/houdini-19.5.682

echo $PWD

# set compiler if the user hasn't explicitly set CC and CXX
if [ -z $CC ]; then
  echo "***NOTE: Defaulting to use clang!"
  echo -n "         Please set env variables 'CC' and 'CXX' to"
  echo " a different supported compiler (gcc/icc) if desired."
  export CC=clang
  export CXX=clang++
fi

#### Build dependencies ####
if [ ! -d "$USD_ROOT" ]
  then
   echo "macosx houdini build requires regular release dependency build"
   exit 2
fi

if [ ! -d "$HOUDINI_ROOT" ]
  then
    cp -r /NAS/packages/apps/usd/macos/houdini-19.5.682 $HOUDINI_ROOT
fi

cd $ROOT_DIR

#### Build HdOSPRay ####

mkdir -p build_release
cd build_release
 Clean out build directory to be sure we are doing a fresh build
rm -rf *
cmake .. -D Houdini_DIR=$HOUDINI_ROOT \
         -D USE_HOUDINI_USD=ON \
         -Dospray_DIR=$USD_ROOT/ospray/lib/cmake/ospray-2.12.0 \
         -Drkcommon_DIR=$USD_ROOT/rkcommon/lib/cmake/rkcommon-1.11.0 \
         -DOpenImageDenoise_DIR=$USD_ROOT/oidn/lib/cmake/OpenImageDenoise-1.4.3 \
         -DTBB_DIR=$USD_ROOT/tbb/lib/cmake/tbb -DCMAKE_BUILD_TYPE=Release \
         -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON \
         -D HDOSPRAY_GENERATE_SETUP=ON \
         -D HDOSPRAY_PYTHON_INSTALL_DIR=/Users/github-runner/Library/Python/3.9 \
         -DHDOSPRAY_SIGN_FILE=$SIGN_FILE_MAC || exit 2
cmake --build . -j ${THREADS} || exit 2

# set release and installer settings
# create installers
make -j $THREADS package || exit 2
cpack -G ZIP || exit 2