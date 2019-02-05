# WIP: USD + Intel OSPRay Alpha Build

 ![OSPRay](/ospray_screenshot.jpg?raw=true "OSPRay")

## Overview
hdOSPRay is a rendering plugin for Hydra+USD using Intel's OSPRay ray tracer.  This is an early release WIP plugin for Hydra that supports ray tracing and path tracing up to real-time speeds.  Please note that this is not yet fully polished and you will likely run into issues.  For help building or running please send me an email at carson.brownlee AT the place I work dot com.  

Currently tested with Linux - Centos 7 and Arch.  

## Limitations
* This is an early release version under development.  Many features will be missing or broken.
* The denoiser is currently only built with a cmake option enabled, but this will only really be possible for outside users when we release it.  Email us to inquire about early access to OpenImageDenoise.
* Picking is currently not supported
* hdLux lights are not supported yet
* usdShade support is implemented in dev branch materials
* subdivision surfaces are currently being put in

## Dependencies
* USD's standard dependancies for core and view if you want to compile with usdview.  
* [USD install](https://github.com/PixarAnimationStudios/USD).  Currently tested with USD dev version git tag 02aeeea277a375968eed01acc68e31d0e24352f4.
* [OSPRay 1.8.x](http://www.ospray.org/)
* [Embree 3.2.x](https://embree.github.io/)
* OpenImageIO is required in usd build
* Optional - OpenImageDenoise
* Optional - ptex.  This is not publicly available yet due to 
*  required changes to core OSPRay.  Email me if you need access.

## Building
* hdOSPRayPlugin uses a cmake build system which links in USD
* download/clone the git repo
```
cd hdOSPRayPlugin
mkdir build
cd build
ccmake ..
```
* enable PXR_BUILD_OSPRAY_PLUGIN
* set pxr_DIR to the install directory of USD
* set ospray_DIR to the directory containing your osprayConfig.cmake.  This can be found in the root directory of the distributed binaries or if you are building and installing from source it can be found in <install>/lib/cmake/ospray-1.7.0/
* set embree_DIR to install directory of embree.  These will be the same as ospray_DIR if you downloaded the ospray binaries.
* set CMAKE_INSTALL_PREFIX to the install directory of USD.
```
make -j install
```
    
## Running
with the plugin built, run usdview and select view->Hydra Renderer->OSPRay.
OSPRay can be set the the default renderer by setting 
* HD_DEFAULT_RENDERER=OSPRay

environment variables to set include:
* HDOSPRAY_SAMPLES_PER_FRAME
* HDOSPRAY_SAMPLES_TO_CONVERGENCE
* HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES
* HDOSPRAY_CAMERA_LIGHT_INTENSITY
* HDOSPRAY_USEPATHTRACING

see config.cpp for a more complete list.
