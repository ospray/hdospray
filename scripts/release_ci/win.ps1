## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

echo "drives: "
 wmic logicaldisk get name
echo "nassie:"
echo "\"
 dir \
echo "pwd:"
pwd

$NAS = "\\vis-nassie.an.intel.com\NAS"
$DEP_DIR = "$NAS\packages\apps\usd\win10"
$ROOT_DIR = pwd

echo "dep_dir:"
ls $DEP_DIR
echo "ospray_dir:"
ls $DEP_DIR\ospray-2.12.0.x86_64.windows\lib\cmake\ospray-2.12.0

## Build dependencies ##
# Windows CI dependencies are prebuilt

cd $ROOT_DIR

#### Build HdOSPRay ####

md build_release
cd build_release

echo "build_release dir:"
pwd
echo "ls dir"
dir

# Clean out build directory to be sure we are doing a fresh build
#rm -r -fo *

cmake -L `
  -D ospray_DIR="$DEP_DIR\ospray-2.12.0.x86_64.windows\lib\cmake\ospray-2.12.0" `
  -D pxr_DIR="$DEP_DIR\usd-23.02" `
  -D rkcommon_DIR="$DEP_DIR\rkcommon\lib\cmake\rkcommon-1.11.0" `
  -D HDOSPRAY_INSTALL_DEPENDENCIES=ON `
  -D HDOSPRAY_SIGN_FILE=$env:SIGN_FILE_WINDOWS `
  ..

cmake --build . --config release -j 32
cmake --build . --config release -j 8 --target PACKAGE
cpack -G ZIP -V

exit $LASTEXITCODE