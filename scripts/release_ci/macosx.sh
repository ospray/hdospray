#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set -x
cmake --version

#### Set variables for script ####

ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps/usd-23.02
THREADS=`sysctl -n hw.logicalcpu`

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
# rm -r *
#### Build dependencies ####
# if [ ! -d "$DEP_DIR/install" ]
# then
  echo "building dependencies"

  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/
  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/
  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/
  mkdir -p /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/bin
  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/
  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/../ispc/bin
  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/../ispc/src/bin
  # ls /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/../ispc/
  # cmake -E copy_directory /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/../ispc/src/bin /Users/github-runner/actions-runner/intel/001/_work/libraries.graphics.renderkit.ospray-hydra/hdospray_deps/usd-23.02/install/bin

  cmake $ROOT_DIR/scripts/superbuild/ -DBUILD_OSPRAY=ON -DBUILD_OSPRAY_ISPC=OFF -DBUILD_HDOSPRAY_ISPC=ON -DBUILD_HDOSPRAY=OFF -DBUILD_USD=OFF -DHDSUPER_USD_VERSION=v23.02 -DBUILD_TIFF=OFF -DBUILD_PNG=OFF -DBUILD_JPEG=OFF -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF . || exit 2
  cmake --build . -j ${THREADS} || exit 2
  echo "libs:"
  ls install/lib
  echo "includes:"
  ls install/include
  #ls install/bin
# else
#   echo "using cached dependencies"
# fi

cd $ROOT_DIR

#### Build OSPRay ####

mkdir -p build_release
cd build_release
# Clean out build directory to be sure we are doing a fresh build
rm -rf *

# Setup environment for dependencies
export CMAKE_PREFIX_PATH=$DEP_DIR

# set release and installer settings

# create installers
#make -j $THREADS package || exit 2