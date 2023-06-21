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

#### Build dependencies ####
if [ ! -d "$DEP_DIR/install" ]
  then
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
  #/mnt/vol/home/visuser/NAS/packages/apps/usd/macos
  #echo "/"
  #ls /
  #echo "/mnt"
  #ls /mnt
  #echo "/mnt/vol"
  #ls /mnt/vol
  #echo "env"
  #env

  #echo "libs:"
  #ls install/lib
  #echo "includes:"
  #ls install/include
  #echo "python:"
  #ls /usr/local/bin
  sw_vers
  pip3.9 list
  echo "PATH:"
  echo $PATH
  echo "\n\n\nPYTHONPATH:"
  echo $PYTHONPATH
  export Python_ROOT_DIR="/usr/local/Frameworks/Python.framework/Versions/3.9"
  # pip3.9 install PySide6 PyOpenGL numpy
  #ls install/bin
  alias python=/usr/local/bin/python3.9
  alias python3=/usr/local/bin/python3.9
  python --version
  python3 --version
  /usr/local/bin/python3.9 --version
  echo "boostjam:"
  cat USD_Super/build/source/boost/python-config.jam
  rm -r *
  export MACOSX_DEPLOYMENT_TARGET=11.7
  cmake $ROOT_DIR/scripts/superbuild/ -DHDSUPER_PYTHON_VERSION=3.9 -DHDSUPER_PYTHON_EXECUTABLE=/usr/local/bin/python3.9 -DBUILD_OSPRAY=ON -DBUILD_OSPRAY_ISPC=OFF -DBUILD_HDOSPRAY_ISPC=ON -DBUILD_HDOSPRAY=OFF -DBUILD_USD=ON -DHDSUPER_USD_VERSION=v23.02 -DBUILD_TIFF=OFF -DBUILD_PNG=OFF -DBUILD_JPEG=OFF -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF .
  cmake --build . -j ${THREADS}

  echo "install/lib:"
  ls install/lib
else
   echo "using cached dependencies"
fi

cd $ROOT_DIR

#### Build HdOSPRay ####

mkdir -p build_release
cd build_release
# Clean out build directory to be sure we are doing a fresh build
rm -rf *
cmake .. -Dpxr_DIR=$USD_ROOT -Dospray_DIR=$USD_ROOT/ospray/lib/cmake/ospray-2.11.0 -Drkcommon_DIR=$USD_ROOT/rkcommon/lib/cmake/rkcommon-1.11.0 -DOpenImageDenoise_DIR=$USD_ROOT/oidn/lib/cmake/OpenImageDenoise-1.4.3 -DTBB_DIR=$USD_ROOT/tbb/lib/cmake/tbb -DCMAKE_BUILD_TYPE=Release || exit 2
cmake --build . -j ${THREADS} || exit 2

# Setup environment for dependencies
export CMAKE_PREFIX_PATH=$DEP_DIR

# set release and installer settings

# create installers
#make -j $THREADS package || exit 2