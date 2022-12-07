#!/bin/sh
## Copyright 2019 Intel Corporation
## SPDX-License-Identifier: Apache-2.0
mkdir build
cd build
rm -rf *

# NOTE(jda) - using Internal tasking system here temporarily to avoid installing TBB
cmake \
-D OSPRAY_ENABLE_TESTING=ON \
-D OSPRAY_AUTO_DOWNLOAD_TEST_IMAGES=OFF \
-D OSPRAY_MODULE_BILINEAR_PATCH=ON \
-D OSPRAY_SG_CHOMBO=OFF \
-D OSPRAY_SG_OPENIMAGEIO=OFF \
-D OSPRAY_SG_VTK=OFF \
..

make -j 4 && make test
