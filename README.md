# OSPRay for Hydra

OSPRay interactive rendering plugin for USD Hydra

<p align="center">

<img src="images/hdospray_alab.jpg" style="padding: 0px 0px 0px 0px; box-shadow: 0px 0px 24px rgba(0,0,0,0.4); ">

</p>

Rendering of the ALab scene using OSPRay for Hydra. The ALab asset is
Copyright 2022 Animal Logic Pty Limited. All rights reserved.

## OSPRay for Hydra

OSPRay for Hydra is an open source plugin for Pixar’s USD to extend the
Hydra rendering framework with [Intel® OSPRay](https://www.ospray.org).
OSPRay for Hydra enables interactive, path traced rendering by utilizing
OSPRay’s renderers and [Intel® Open Image
Denoise](http://openimagedenoise.org). OSPRay for Hydra and OSPRay are
released under the permissive Apache 2.0 license.

As part of the [Intel oneAPI Rendering
Toolkit](https://software.intel.com/en-us/rendering-framework), OSPRay
is highly-optimized for Intel® CPU architectures ranging from laptops to
large-scale distributed HPC systems. OSPRay for Hydra leverages the
Intel® Rendering Framework to deliver interactive rendering for
large-scale models at high levels of fidelity, as demonstrated at
SIGGRAPH 2018 using over [100GB of production
assets](https://itpeernetwork.intel.com/intel-open-source-libraries-hollywood).

### Support and Contact

OSPRay for Hydra is still in beta, with new features and existing issues
being worked on regularly. USD is also under active development and is
often fast changing. We tag the OSPRay for Hydra releases to match
specified USD releases, but cannot always anticipate all of the changes
and resulting issues for users. Please report any issues you may run
into to our [issue tracker](https://github.com/ospray/hdospray/issues).
We always welcome suggestions and especially pull requests\!

# Building OSPRay for Hydra

OSPRay for Hydra source is available on GitHub at [OSPRay for
Hydra](http://github.com/ospray/hdospray). The master branch is
typically the most stable branch and contains tagged releases.

Tags are of the form `hdospray-vx.x.x-usdvx.x.x`, with `vx.x.x` being
the release of OSPRay for Hydra and `usdv` being the version of USD it
is built against. This is required due to the frequently changing
internals of hydra.

Currently OSPRay for Hydra is regularly tested on Ubuntu 22.04, and has
been tested on Windows 10 and MacOS 13.6.

## Prerequisites

USD has a large number of dependencies depending on your configuration.
We provide a superbuild for linux/mac which builds USD and other
dependencies. At a base you will need the following system libraries,
though you may need others depending on what USD modules you are
building and what system you are running: - c/c++ compiler (gcc 6.3.1+)
- cmake 3.1.1+ - (for USD python support including usdview) python
(3.7.x+ recommended), with PySide2/PySide6, numpy, and PyOpenGL. ‘pip
install PySide6 PyOpenGL’

If you are building standalone, you will need: -
[USD 23.02, 22.08, 21.08,
or 20.08](https://graphics.pixar.com/usd/docs/index.html) - Other USD
versions between these discrete releases may work, but are untested. -
For a full list of USD dependencies, see the USD page. -
[OSPRay 3.0.0](http://www.ospray.org/) - We recommend using ospray’s
superbuild to build dependencies such as embree, ospcommon, and openvkl.
OpenImageDenoise can also be enabled through superbuild. - rkcommon is a
library dependency of OSPRay and hdOSPRay, and built as part of OSPRay’s
superbuild.

## Optional Dependencies

  - Houdini SDK (tested against 18.5, 19.0, 19.5). To use, enable
    SUPERBUILD\_USE\_HOUDINI in superbuild.
  - [OpenImageDenoise](https://github.com/OpenImageDenoise/oidn.git)
      - Open Image Denoise needs be be enabled in the OSPRay build.

## Superbuild on Linux/MacOS

OSPRay for Hydra contains a cmake superbuild script that builds external
dependencies for you and is the recommended way of building OSPRay for
Hydra on Linux/Mac. Alternatively, instructions for manually building
each component is also given. Currently, USD 23.02 is the default.

    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=<install_dir> ../scripts/superbuild/ (or use ccmake to specify option through gui)
    cmake --build . -j <numthreads>

By default, all install files will be installed into
<build dir>/install. A setup script can then be called to set paths:

    source <install_dir>/setup_hdospray.sh
    usdview <usdfile> --renderer OSPRay

## Manual Compiling USD on Linux/MacOS

To build USD, see the [USD GitHub
site](https://github.com/PixarAnimationStudios/USD). If you wish to use
usdview, you must also enable imaging and python.  
The options and compilers used can vary from our example, but make sure
that TBB use is consistent across your build of USD, OSPRay for Hydra,
and OSPRay. The command we use for building USD manually is:

    python <USD_SOURCE>/build_scripts/build_usd.py --python --usd-imaging --openimageio <USD_BUILD_DIR>

To set TBB explicitly, go to `<USD_BUILD_DIR>`/build/USD and set TBB
libraries and include directories using cmake.

## Compiling OSPRay on Linux/MacOS

You can use the distributed binaries of OSPRay or build it yourself. We
recommend using the OSPRay superbuild system according the instructions
listed on [github](https://github.com/ospray/OSPRay). Make sure that TBB
is the same used by USD. You can force using system TBB using the
superbuild by going to `<OSPRAY_BUILD_DIR>`, and setting the cmake
variable `DOWNLOAD_TBB` to OFF. GPU Support needs to be manually
enabled, see OSPRay docs.

## Compiling OSPRay for Hydra on Linux/MacOS

OSPRay for Hydra plugin uses a CMake build system which links in USD and
builds externally from the USD source directory, but configures CMake as
if it were inside the USD repository in order to build the plugin using
USD internals. It must therefore be built against a version of USD that
matches OSPRay for Hydra, which is specified in the versioning tag of
OSPRay for Hydra.

  - Download/clone the git repo for OSPRay for Hydra
    
        $ git clone https://github.com/ospray/hdospray.git

  - Create a build directory and call CMake
    
        $ cd hdospray
        $ mkdir build
        $ cd build
        $ ccmake ..

  - Set `CMAKE_INSTALL_PREFIX` to the installation directory of USD.

  - Set `pxr_DIR` to the install directory of USD which contains
    `pxrConfig.cmake`

  - Set `ospray_DIR` to the directory containing your
    `osprayConfig.cmake`
    
      - This can be found in the root directory of the distributed
        binaries or if you are building and installing from source it
        can be found in `<install>/lib/cmake/ospray-\*/`

  - Set `openvkl_DIR` to install directory of OpenVKL. These will be the
    same as `ospray_DIR` if you downloaded the OSPRay binaries, or in
    OSPRay’s superbuild directory under
    install/openvkl/lib/cmake/openvkl\*.

  - Set `rkcommon_DIR` to install directory of rkCommon. These will be
    the same as `ospray_DIR` if you downloaded the OSPRay binaries, or
    in OSPRay’s superbuild directory under
    install/rkcommon/lib/cmake/rkcommon\*.

  - Set `embree_DIR` to install directory of Embree. These will be the
    same as `ospray_DIR` if you downloaded the OSPRay binaries, or in
    OSPRay’s superbuild directory under
    install/embree/lib/cmake/embree\*.

  - Compile and install OSPRay for Hydra
    
        $ make -j install

The plugin should now be in `<usd install
directory>/plugin/usd/hdOSPRay`

## Compiling OSPRay for Hydra on Windows

Compliation on windows is similar to Linux/Mac.

# Documentation

## Running

Once built, the plugin code should be located in your `usd` install
directory under `plugin/usd/HdOSPRay`. Run `usdview <scenefile>` and
select `view->Hydra` and then `Renderer->OSPRay`.

OSPRay can be set to the default renderer by either

  - Setting the `HD_DEFAULT_RENDERER` environment variable
    
        $ export HD_DEFAULT_RENDERER=OSPRay

  - Specifying `--renderer OSPRay` as a command line argument to usdview

### Environment variables

Most of these options are also exposed through the usdview GUI under
Hydra Settings.

  - `HDOSPRAY_DEVICE`
    
    cpu (default) or gpu device.

  - `HDOSPRAY_USE_DENOISER`
    
    If built in, enable the denoiser

  - `HDOSPRAY_SAMPLES_PER_FRAME`
    
    Number of samples per pixel.

  - `HDOSPRAY_SAMPLES_TO_CONVERGENCE`
    
    Will progressively render frames until this many samples per pixel,
    then stop rendering.

  - `HDOSPRAY_INTERACTIVE_TARGET_FPS`
    
    Set interactive scaling to match target fps when interacting. 0
    Disables interactive scaling.

  - `HDOSPRAY_LIGTH_SAMPLES`

Number of light samples at every path intersection. A value of -1 leads
to sampling all light sources in the scene. *Does not affect scivis
renderer.*

  - `HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES`
    
    Number of ambient occlusion samples to compute per pixel. *Does not
    affect path tracer.*

  - `HDOSPRAY_CAMERA_LIGHT_INTENSITY`
    
    Globally modify the intensity of all lights.

  - `HDOSPRAY_USE_PATH_TRACING`
    
    Use Monte Carlo path tracer instead of faster Whitted-style
    renderer.

  - `HDOSPRAY_MAX_PATH_DEPTH`
    
    Maximum ray depth. Will affect the number of diffuse bounces as well
    as transparency.

  - `HDOSPRAY_USE_SIMPLE_MATERIAL`
    
    Use a simple diffuse + phone material instead of principled
    material.

  - `HDOSPRAY_INIT_ARGS`
    
    Specify arguments sent on to OSPRay for initialization. e.g. enable
    MPI.

  - `HDOSPRAY_PIXELFILTER_TYPE`
    
    The type of pixel filter used by OSPRay: 0 (point), 1 (box), 2
    (gauss), 3 (mitchell), and 4 (blackmanHarris).

  - `HDOSPRAY_FORCE_QUADRANGULATE`
    
    Force Quadrangulate meshes for debug.

## Features

  - Denoising using [Open Image Denoise](http://openimagedenoise.org)
  - Distributed multi-node rendering over MPI
  - UVTextures
  - Pixel Filters
  - usdLux lights: disk, distant, dome, quad, sphere.
  - Triangle meshes
  - Quad meshes
  - Subdivision surfaces
  - Ray traced shadows and ambient occlusion.
  - Path tracing
  - Physically-based materials
  - Principled shader (similar to Disney BSDF shader)

# News, Updates, and Announcements

  - Oct 25, 2023: Version v1.0.0
    
        - USD 23.02 support
        - Superbuild now part of core repo
        - Macosx and Windows build support
        - Fixes for Windows DLL loading
        - Houdini binary release builds
        - Resource loading now uses Hio
        - Openimageio dependency removal
        - Support for loading usdz files directly
        - Custom OSPRay materials
        - Changes to interactive rendering mode for smoother camera movement
        - Depth buffer transform to non-linear for DCC compositing
        - Various bug fixes

  - Jan 5, 2023: Version v0.11.0
    
        - Texture Transforms
        - UDIM textures
        - Texture channel masks
        - Tonemapper settings
        - Interactive rendering reworked
        - Bug fixes

  - Sep 20, 2022: Version v0.10.1
    
        - USD 22.08 support. 21.08 is still default in superbuild for houdini. 
        - Golden image tests added
        - Github actions workflow added for CI
        - Bug fixes

  - June 10, 2022: Version v0.10
    
        - USD 21.08 support, which is now the default in superbuild.  20.08 still supported if enabled.
        - Instance, Element, and Primitive ID buffers added
        - Testing added with image comparisons
        - Added facevarying texture and color support.

  - Feb 15, 2022: Version v0.9
    
        - Adding depth AOV
        - Adding normals AOV
        - Additional material parameters
        - Fixing generic texcoord naming
        - Update to OSPRay 2.9
        - Adding cylinder light
        - Adding camera depth of field
        - Minimal support for geomsubset materials
        - Ptex support currently deprecated in OSPRay.  Will be re-enabled upon OSPRay module_ptex release.

  - August 24, 2021: Version v0.8
    
        - Update to USD 20.08.
        - Houdini support.
        - Color buffer AOV support.
        - Superbuild script added.
        - Added camera light visibility flags.
        - Add thin materials.

  - May 28, 2021: Version v0.7
    
        - Update to OSPRay 2.6.0
        - Ptex support re-enabled.
        - Numerous light and material updates. Added disk lights.
        - Toggle for light enabled vs visible.
        - Animation update fixes.
        - Support for curves.

  - October 12, 2020: Version v0.6
    
        - Update to OSPRay 2.4.0
        - Added support for opacity textures.
        - Added adaptive view port scaling. The viewport is
          now scaled to reach a given interactive framerate (e.g., 10fps).
        - Fixed some bigs with animated shapes (e.g., ARKit data sets).
        - Added support to toggle the visibility of light sources and shapes.

  - July 31, 2020: Version v0.5
    
      - Update to OSPRay 2.2.0.
      - Added UsdLux light support.
      - Asynchronous rendering.
      - Dynamic resolution for greater interaction.
      - Pixel Filters.
      - Subdivision surfaces interpolation mode fixes.

  - June 15, 2020: Version v0.4
    
      - Update to OSPRay 2.1.0.
      - Update to USD 20.05.

  - September 15, 2019: Version v0.3
    
      - Subdivision surfaces support.
      - GUI options for usdview.
      - Various bug fixes.

  - April 30, 2019: Version v0.2.2
    
      - Various bug fixes.
      - OSPRay version updated to 1.8.5.
      - CMake targets.
      - Added animation support.

  - Mar 7, 2019: Version v0.2.1
    
      - Bug fixes.
      - Ptex updates.
      - Documentation.

  - Feb 28, 2019: Version v0.2.0
    
      - Initial Beta release version 0.2.0 is now available on the
        [HdOSPRay GitHub
        page](https://github.com/ospray/hdospray/releases/v0.2.0). This
        replaces the previous repository and versioning which was on my
        personal github account.
