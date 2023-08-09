## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

pip3.8 install PyOpenGL
pip3.8 install PySide2
pip3.8 install numpy
echo "pip3 show PyOpenGL:"
pip3.8 show PyOpenGL
echo "drives: "
 wmic logicaldisk get name
echo "nassie:"
echo "\"
 dir \
echo "pwd:"
pwd

$NAS = "\\vis-nassie.an.intel.com\NAS"
$NAS_DEP_DIR = "$NAS\packages\apps\usd\win10"
$ROOT_DIR = pwd
$DEP_DIR = "../hdospray_deps/usd-23.02"
md $DEP_DIR

echo "nas_dep_dir:"
ls $NAS_DEP_DIR
echo "dep_dir:"
ls $DEP_DIR
echo "ospray_dir:"
ls $NAS_DEP_DIR\ospray-2.12.0.x86_64.windows\lib\cmake\ospray-2.12.0
echo "ls c:\\program files\\python38\\"
ls "C:\Program Files"
ls "C:\Program Files\Python38"
#echo "ls C:\\Python39:"
#ls C:\Python39
#echo "ls C:\\Python39\\lib:"
#ls C:\Python39\lib
echo "ls C:\\Program Files\Python38\\lib\\site-packages:"
ls "C:\Program Files\Python38\lib\site-packages"
echo "ls ~/AppData: "
ls ~\AppData
#echo "ls ~/AppData/Local/Programs/Python:"
#echo "ls ~/AppData/Roaming/Programs:"
#ls ~/AppData/Roaming/Programs

## Build dependencies ##
# Windows CI dependencies are prebuilt

cd $ROOT_DIR

#### Build HdOSPRay ####

md build_release
cd build_release

echo "build_release dir:"
pwd
cp $NAS\packages\apps\usd\win10\houdini-19.5.6bs82\python39\libs/python39.lib .
echo "ls dir"
dir

$env:Path += ";$NAS\packages\apps\usd\win10\houdini-19.5.6bs82\python39\libs"
$env:LIB += ";$NAS\packages\apps\usd\win10\houdini-19.5.682\python39\libs"
$env:LIBPATH += ";$NAS\packages\apps\usd\win10\houdini-19.5.682\python39\libs"

# Clean out build directory to be sure we are doing a fresh build
#rm -r -fo *

cmake -L `
  -D ospray_DIR="$NAS_DEP_DIR\ospray-2.12.0.x86_64.windows\lib\cmake\ospray-2.12.0" `
  -D Houdini_DIR="$NAS\packages\apps\usd\win10\houdini-19.5.682\toolkit\cmake" `
  -D rkcommon_DIR="$NAS_DEP_DIR\rkcommon\lib\cmake\rkcommon-1.11.0" `
  -D HDOSPRAY_INSTALL_OSPRAY_DEPENDENCIES=OFF `
  -D HDOSPRAY_INSTALL_PYTHON_DEPENDENCIES=OFF `
  -D HDOSPRAY_GENERATE_SETUP=OFF `
  -D HDOSPRAY_SIGN_FILE=$env:SIGN_FILE_WINDOWS `
  -D USE_HOUDINI_USD=ON `
  ..
  #-D HDOSPRAY_PYTHON_INSTALL_DIR="C:\Program Files\Python38" `
  #-D PYTHON_VERSION=3.8 `

cmake --build . --config release -j 32
#cmake --build . --config release -j 32 --target PACKAGE
#cpack -G ZIP

exit $LASTEXITCODE