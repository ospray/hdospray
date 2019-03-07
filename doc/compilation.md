Building HdOSPRay
=======================================

HdOSPRay source is available on GitHub at 
[HdOSPRay](http://github.com/ospray/hdospray).  The master branch
is typically the most stable branch and contains tagged releases.
Tags are of the form hdospray-vx.x.x-usdvx.x.x, with vx.x.x being
the release of HdOSPRay and usdv being the version of USD it is built
against.  This is required due to the often changing internals of hydra 
calls in USD.  Currently HdOSPRay is regularly tested on linux, which is
the primary development target of USD itself.  MacOS support in USD is 
experimental, but we often test against it. Windows support of USD is
 experimental, but we have not tested HdOSPRay with it so it is use 
 at your own risk.

Prerequisites
-------------

- [USD v19.03](https://graphics.pixar.com/usd/docs/index.html)
  - USD is primarily tested with Linux, but has experimental support for MacOS and Windows.
    For a full list of USD dependencies, see the USD page.
  - The plugin requires a minimum of PXR_BUILD_IMAGING and
    PXR_BUILD_OPENIMAGEIO_PLUGIN to be set to ON for USD.
- [OSPRay 1.8.x](http://www.ospray.org/)
- [Embree 3.2.x](https://embree.github.io/)
- CMake 3.1.1+

# Optional Dependancies
- [OpenImageDenoise](https://github.com/OpenImageDenoise/oidn.git)
        -  Open Image Denoise also needs be be enabled in the OSPRay build
- [Ptex](https://github.com/wdas/ptex)
        - Ptex will need to be enabled in the USD build
        - Can be downloaded and built by the USD build_usd.py script
        - [Ptex module](https://github.com/ospray/module_ptex) also needs to be
      enabled in the OSPRay build

Compiling HdOSPRay on Linux/macOS
-------------------------------------------

HdOSPRay plugin uses a cmake build system which links in USD and builds
externally from the USD source directory, but configures cmake as if it were
inside the USD repository in order to build the plugin using USD internals. It
must therefore be built against a version of USD that matches HdOSPRay, which is
specified in the versioning tag of HdOSPRay. To build USD, see the [USD GitHub
site](https://github.com/PixarAnimationStudios/USD). We recommend following the
build scripts provided.

* download/clone the git repo for HdOSPRay
```
git clone https://github.com/ospray/hdospray.git
```
* create a build directory and call cmake
```
cd hdospray
mkdir build
cd build
ccmake ..
```
* set pxr_DIR to the install directory of USD which contains pxrConfig.cmake
* set required USD options: `usd-imaging` and `openimageio` are required for
    both the USD and HdOSPRay builds.
* set ospray_DIR to the directory containing your osprayConfig.cmake.  
    -   This can be found in the root directory of the distributed binaries or
        if you are building and installing from source it can be found in
        `<install>/lib/cmake/ospray-\*/`
-   set embree\_DIR to install directory of embree. These will be the same as
    ospray\_DIR if you downloaded the ospray binaries.

```
make -j install
```
* the plugin should now be in \<usd install directory>/plugin/usd/hdOSPRay

Compiling HdOSPRay on Windows
---------------------------------------

Windows support of USD is
 experimental.  We have not tested HdOSPRay with it, so use 
 at your own risk.
