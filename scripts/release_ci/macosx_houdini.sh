#!/bin/bash
## Copyright 2023 Intel Corporation
## SPDX-License-Identifier: Apache-2.0


set -x
cmake --version

#### Set variables for script ####

ROOT_DIR=$PWD
DEP_DIR=$ROOT_DIR/../hdospray_deps
THREADS=`sysctl -n hw.logicalcpu`
USD_ROOT=$DEP_DIR/usd-23.02
OSPRAY_ROOT=$DEP_DIR/ospray-3.1.0
HOUDINI_ROOT=$DEP_DIR/houdini-19.5.682

echo "usrbinpip: "
ls /usr/bin/pip*

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
mkdir -p $OSPRAY_ROOT
rm -r $OSPRAY_ROOT/*
if [ ! -d "$OSPRAY_ROOT/install" ]
  then
    echo "building dependencies"
    pip3.9 install PySide6
    pip3.9 install PySide6-Addons
    pip3.9 install PySide6-Essentials
    echo "which pyside6-uic:"
    which pyside6-uic
    pip3.9 show PySide6
    echo "site-packages:"
    ls /usr/local/lib/python3.9/site-packages
    echo "site-packages2:"
    ls /Users/github-runner/Library/Python/3.9/lib/python/site-packages
    echo "libexec: "
    mkdir -p $OSPRAY_ROOT/install
    mkdir -p $OSPRAY_ROOT/install/embree
    mkdir -p $OSPRAY_ROOT/install/bin
    cd $OSPRAY_ROOT
    export Python_ROOT_DIR="/usr/local/Frameworks/Python.framework/Versions/3.9"
    export Python3_ROOT_DIR="/usr/local/Frameworks/Python.framework/Versions/3.9"
    alias python=/usr/local/bin/python3.9
    alias python3=/usr/local/bin/python3.9
    export MACOSX_DEPLOYMENT_TARGET=11.7
    cmake $ROOT_DIR/scripts/superbuild/ -DHDSUPER_PYTHON_VERSION=3.9 \
      -D CMAKE_BUILD_TYPE=Release \
      -DHDSUPER_PYTHON_EXECUTABLE=/usr/local/bin/python3.9 -DHDSUPER_OSPRAY_USE_EXTERNAL=OFF \
      -DBUILD_OSPRAY=ON -DBUILD_OSPRAY_ISPC=ON -DBUILD_HDOSPRAY_ISPC=OFF -DBUILD_HDOSPRAY=OFF \
      -DBUILD_USD=OFF -DHDSUPER_USD_VERSION=v23.08 -DBUILD_TIFF=OFF -DBUILD_PNG=OFF \
      -DBUILD_BOOST=OFF -DPYSIDE_BIN_DIR=/Users/github-runner/Library/Python/3.9/lib/python/site-packages/PySide6/Qt/libexec \
      -DBUILD_JPEG=OFF -DBUILD_PTEX=OFF -DENABLE_PTEX=OFF -DCMAKE_INSTALL_PREFIX=$OSPRAY_ROOT/install
    cmake --build . -j ${THREADS}
    #|| (rm -r install ; exit 2)
    #make install -j ${THREADS}
    echo "dep dir: "
    ls $OSPRAY_ROOT
    echo "dep dir ospray: "
    ls $OSPRAY_ROOT/OSPRay
    echo "dep install dir: "
    ls $OSPRAY_ROOT/install
    echo "dependency build completed"
    echo "rkcommon: "
    ls $OSPRAY_ROOT/install/rkcommon/lib/
    ls $OSPRAY_ROOT/install/rkcommon/lib/cmake/
    cd $ROOT_DIR
fi

#rm -rf $HOUDINI_ROOT
if [ ! -d "$HOUDINI_ROOT" ]
  then
    cp -r $STORAGE_PATH/packages/apps/usd/macos/houdini-19.5.682 $HOUDINI_ROOT
fi
# houdini framework seems to reference incorrect library locations
ln -s $HOUDINI_ROOT/Libraries $HOUDINI_ROOT/Frameworks/Houdini.framework/Versions/19.5/Libraries

echo "houdini_root:"
ls $HOUDINI_ROOT

cd $ROOT_DIR

#### Build HdOSPRay ####

mkdir -p build_release
cd build_release
# Clean out build directory to be sure we are doing a fresh build
rm -rf *
echo "ospray_DIR:"
cmake .. -D Houdini_DIR=$HOUDINI_ROOT/Resources/toolkit/cmake/ \
         -D CMAKE_BUILD_TYPE=Release \
         -D USE_HOUDINI_USD=ON \
         -Dospray_DIR=$OSPRAY_ROOT/install/ospray/lib/cmake/ospray-3.1.0 \
         -Drkcommon_DIR=$OSPRAY_ROOT/install/rkcommon/lib/cmake/rkcommon-1.13.0 \
         -DTBB_DIR=$OSPRAY_ROOT/install/tbb/lib/cmake/tbb -DCMAKE_BUILD_TYPE=Release \
         -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=ON \
         -D HDOSPRAY_GENERATE_SETUP=ON \
         -D HDOSPRAY_PYTHON_INSTALL_DIR=/Users/github-runner/Library/Python/3.9 \
         -D CMAKE_BUILD_WITH_INSTALL_RPATH=ON \
         -D CMAKE_INSTALL_PREFIX=/opt/local \
         -D CMAKE_INSTALL_INCLUDEDIR=include \
         -D CMAKE_INSTALL_LIBDIR=lib \
         -D CMAKE_INSTALL_BINDIR=bin \
         -D CMAKE_MACOSX_RPATH=ON \
         -D CMAKE_INSTALL_RPATH=$ROOT_DIR/build_release/install || exit 2
         # disabling mac signing, signing currently broken on macs
         #-DHDOSPRAY_SIGN_FILE=$SIGN_FILE_MAC || exit 2

# set release and installer settings
# create installers
make -j $THREADS package || exit 2
cmake -L -D HDOSPRAY_ZIP_MODE=ON .
# for some reason, rpaths are failing to be set only on libtbb
install_name_tool -change /usr/local/opt/tbb/lib/libtbb.12.dylib @rpath/libtbb.12.dylib plugin/usd/hdOSPRay.dylib
install_name_tool -change /usr/local/opt/tbb/lib/libtbbmalloc.2.dylib @rpath/libtbbmalloc.2.dylib plugin/usd/hdOSPRay.dylib
make -j $THREADS package || exit 2
# make -j $THREADS package || exit 2
# cpack -G ZIP || exit 2
