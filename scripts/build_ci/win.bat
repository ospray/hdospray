@echo off
rem Copyright 2019 Intel Corporation
rem SPDX-License-Identifier: Apache-2.0setlocal

md build
cd build
cmake -L ^
-G "%~1" ^
-T "%~2" ^
-D OSPRAY_BUILD_ISA=ALL ^
-D OSPRAY_ENABLE_TESTING=ON ^
-D OSPRAY_AUTO_DOWNLOAD_TEST_IMAGES=OFF ^
-D OSPRAY_MODULE_BILINEAR_PATCH=ON ^
-D OSPRAY_MODULE_MPI="%~3" ^
-D OSPRAY_SG_CHOMBO=OFF ^
-D OSPRAY_SG_OPENIMAGEIO=OFF ^
-D OSPRAY_SG_VTK=OFF ^
..

cmake --build . --config Release --target ALL_BUILD -- /m /nologo ^
  && ctest . -C Release

:abort
endlocal
:end

rem propagate any error to calling PowerShell script:
exit
