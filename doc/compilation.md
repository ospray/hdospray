# Building OSPRay for Hydra

OSPRay for Hydra source is available on GitHub at
[OSPRay for Hydra](http://github.com/ospray/hdospray). The master branch is typically
the most stable branch and contains tagged releases.

Tags are of the form `hdospray-vx.x.x-usdvx.x.x`, with `vx.x.x` being the
release of OSPRay for Hydra and `usdv` being the version of USD it is built against.
This is required due to the frequently changing internals of hydra.

Currently OSPRay for Hydra is regularly tested on Ubuntu 22.04, and has been tested on Windows 10 and MacOS 13.6.

## Prerequisites

USD has a large number of dependencies depending on your configuration.  We provide a superbuild for linux/mac which builds USD and other dependencies.  At a base you will need the following system libraries, though you may need others depending on what USD modules you are building and what system you are running:
- c/c++ compiler (gcc 6.3.1+)
- cmake 3.1.1+
- (for USD python support including usdview) python (3.7.x+ recommended), with PySide2/PySide6, numpy, and PyOpenGL.  'pip install PySide6 PyOpenGL'

If you are building standalone, you will need:
- [USD 23.02, 22.08, 21.08, or 20.08](https://graphics.pixar.com/usd/docs/index.html)
  - Other USD versions between these discrete releases may work, but are untested.
  - For a full list of USD dependencies, see the USD page.
  - [OSPRay 3.0.0](http://www.ospray.org/)
      - We recommend using ospray’s superbuild to build dependencies
        such as embree, ospcommon, and openvkl.  OpenImageDenoise can
        also be enabled through superbuild.
      - rkcommon is a library dependency of OSPRay and hdOSPRay, and built as part of OSPRay's superbuild.

## Optional Dependencies

  - Houdini SDK (tested against 18.5, 19.0, 19.5). To use, enable SUPERBUILD_USE_HOUDINI in superbuild.
  - [OpenImageDenoise](https://github.com/OpenImageDenoise/oidn.git)
      - Open Image Denoise needs be be enabled in the OSPRay build.

## Superbuild on Linux/MacOS

OSPRay for Hydra contains a cmake superbuild script that builds external dependencies for you
and is the recommended way of building OSPRay for Hydra on Linux/Mac. Alternatively, instructions for
manually building each component is also given.  Currently, USD 23.02 is the default.

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=<install_dir> ../scripts/superbuild/ (or use ccmake to specify option through gui)
cmake --build . -j <numthreads>
```

By default, all install files will be installed into <build dir>/install.
A setup script can then be called to set paths:

```
source <install_dir>/setup_hdospray.sh
usdview <usdfile> --renderer OSPRay
```

## Manual Compiling USD on Linux/MacOS

To build USD, see the [USD GitHub
site](https://github.com/PixarAnimationStudios/USD). 
If you wish to use usdview, you must also enable imaging and python.  
The options and compilers used can vary from our example,
 but make sure that TBB use is consistent across your build of USD, 
 OSPRay for Hydra, and OSPRay.  The command we use for building USD manually is:
```
python <USD_SOURCE>/build_scripts/build_usd.py --python --usd-imaging --openimageio <USD_BUILD_DIR>
```

To set TBB explicitly, go to `<USD_BUILD_DIR>`/build/USD and set TBB libraries and include directories using cmake. 

## Compiling OSPRay on Linux/MacOS

You can use the distributed binaries of OSPRay or build it yourself.
We recommend using the OSPRay superbuild system according the instructions listed on [github](https://github.com/ospray/OSPRay).  Make sure that TBB is the same used by USD.  You can force using system TBB using the superbuild by going to `<OSPRAY_BUILD_DIR>`, and setting the cmake variable `DOWNLOAD_TBB` to OFF. GPU Support
needs to be manually enabled, see OSPRay docs.

## Compiling OSPRay for Hydra on Linux/MacOS

OSPRay for Hydra plugin uses a CMake build system which links in USD and builds
externally from the USD source directory, but configures CMake as if it were
inside the USD repository in order to build the plugin using USD internals. It
must therefore be built against a version of USD that matches OSPRay for Hydra, which is
specified in the versioning tag of OSPRay for Hydra.

- Download/clone the git repo for OSPRay for Hydra

    ```
    $ git clone https://github.com/ospray/hdospray.git
    ```

- Create a build directory and call CMake

    ```
    $ cd hdospray
    $ mkdir build
    $ cd build
    $ ccmake ..
    ```

- Set `CMAKE_INSTALL_PREFIX` to the installation directory of USD.
- Set `pxr_DIR` to the install directory of USD which contains `pxrConfig.cmake`
- Set `ospray_DIR` to the directory containing your `osprayConfig.cmake`
    - This can be found in the root directory of the distributed binaries or
      if you are building and installing from source it can be found in
      `<install>/lib/cmake/ospray-\*/`
- Set `openvkl_DIR` to install directory of OpenVKL. These will be the same as
  `ospray_DIR` if you downloaded the OSPRay binaries, or in OSPRay's 
  superbuild directory under install/openvkl/lib/cmake/openvkl*.
- Set `rkcommon_DIR` to install directory of rkCommon. These will be the same as
  `ospray_DIR` if you downloaded the OSPRay binaries, or in OSPRay's 
  superbuild directory under install/rkcommon/lib/cmake/rkcommon*.
- Set `embree_DIR` to install directory of Embree. These will be the same as
  `ospray_DIR` if you downloaded the OSPRay binaries, or in OSPRay's 
  superbuild directory under install/embree/lib/cmake/embree*.
- Compile and install OSPRay for Hydra

    ```
    $ make -j install
    ```

The plugin should now be in `<usd install directory>/plugin/usd/hdOSPRay`

## Compiling OSPRay for Hydra on Windows

Compliation on windows is similar to Linux/Mac.
