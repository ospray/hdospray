# Building OSPRay for Hydra

OSPRay for Hydra source is available on GitHub at
[HdOSPRay](http://github.com/ospray/hdospray). The master branch is typically
the most stable branch and contains tagged releases.

Tags are of the form `hdospray-vx.x.x-usdvx.x.x`, with `vx.x.x` being the
release of HdOSPRay and `usdv` being the version of USD it is built against.
This is required due to the frequently changing internals of hydra.

Currently HdOSPRay is regularly tested on Linux Arch.  MacOS support in USD is 
experimental, but we often test against it. Windows support of USD is
also experimental, but we have not tested HdOSPRay with it.

## Prerequisites

If using the superbuild, all listed dependencies are downloaded for you. 

- [USD v20.08](https://graphics.pixar.com/usd/docs/index.html)
  - USD is primarily tested with Linux, but has experimental support for MacOS and Windows.
    For a full list of USD dependencies, see the USD page.
  - [OSPRay 2.6.0](http://www.ospray.org/)
      - We recommend using ospray’s superbuild to build dependencies
        such as embree, ospcommon, and openvkl.  OpenImageDenoise can
        also be enabled through superbuild.
  - [OpenImageIO 1.8.17](https://sites.google.com/site/openimageio/home)
  - CMake 3.1.1+

## Optional Dependencies

  - [OpenImageDenoise](https://github.com/OpenImageDenoise/oidn.git)
      - Open Image Denoise needs be be enabled in the OSPRay build.
  - [Ptex](https://github.com/wdas/ptex)
      - [Ptex module](https://github.com/ospray/module_ptex) needs
        to be enabled in the OSPRay build and the library accessible 
        on library paths

## Superbuild on Linux/MacOS

HdOSPRay contains a cmake superbuild script that builds external dependencies for you
and is the recommended way of building hdospray.  By default, this will also build
usdview.

```
mkdir build
cd build
cmake ../scripts/superbuild/ (or use ccmake to specify option through gui)
cmake --build . -j <numthreads>
```

By default, all install files will be installed into <build dir>/install.
A setup script can then be called to set paths:

```
source <build dir>/install/bin/setup_hdospray.sh
usdview <usdfile> --renderer OSPRay
```


## Manual Compiling USD on Linux/MacOS

To build USD, see the [USD GitHub
site](https://github.com/PixarAnimationStudios/USD). 
We recommend following the
build scripts provided, for which we provide an example invocation below.  
If you wish to use usdview, you must also enable imaging and python.  
The options and compilers used can vary from our example,
 but make sure that TBB use is consistent across your build of USD, 
 hdOSPRay, and OSPRay.  The command we use for building USD is:

```
python <USD_SOURCE>/build_scripts/build_usd.py --python --usd-imaging --openimageio --ptex <USD_BUILD_DIR>
```

To set TBB explicitly, go to `<USD_BUILD_DIR>`/build/USD and set TBB libraries and include directories using cmake. 

## Compiling OSPRay on Linux/MacOS

You can use the distributed binaries of OSPRay or build it yourself.
We recommend using the OSPRay superbuild system according the instructions listed on [github](https://github.com/ospray/OSPRay).  Make sure that TBB is the same used by USD.  You can force using system TBB using the superbuild by going to `<OSPRAY_BUILD_DIR>`, and setting the cmake variable `DOWNLOAD_TBB` to OFF.

## Compiling HdOSPRay on Linux/MacOS

HdOSPRay plugin uses a CMake build system which links in USD and builds
externally from the USD source directory, but configures CMake as if it were
inside the USD repository in order to build the plugin using USD internals. It
must therefore be built against a version of USD that matches HdOSPRay, which is
specified in the versioning tag of HdOSPRay.

- Download/clone the git repo for HdOSPRay

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
- Compile and install HdOSPRay

    ```
    $ make -j install
    ```

The plugin should now be in `<usd install directory>/plugin/usd/hdOSPRay`

## Compiling HdOSPRay on Windows

Windows support of USD is experimental.  We have not tested HdOSPRay with it,
so use at your own risk.
