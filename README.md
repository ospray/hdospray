# HdOSPRay

OSPRay interactive rendering plugin for USD Hydra

<img src="images/hdospray_kitchen_pt.jpg" style="padding: 0px 0px 0px 0px; box-shadow: 0px 0px 24px rgba(0,0,0,0.4);">

Pixar’s Kitchen asset rendered with OSPRay’s path tracer at interactive
rates.

Visit [HdOSPRay on github](https://github.com/ospray/hdospray) for more
information.

## HdOSPRay

HdOSPRay is an open source plugin for Pixar’s USD to extend USD’s Hydra
rendering framework with [Intel® OSPRay](https://www.ospray.org).
HdOSPRay enables interactive scene preview by utilizing OSPRay’s high
quality renderers and [Intel® Open Image
Denoise](http://openimagedenoise.org), and is released under the
permissive Apache 2.0 license.

As part of the [Intel oneAPI Rendering
Toolkit](https://software.intel.com/en-us/rendering-framework), OSPRay
is highly-optimized for Intel® CPU architectures ranging from laptops to
large-scale distributed HPC systems. HdOSPRay leverages the Intel®
Rendering Framework to deliver interactive rendering for large-scale
models at high levels of fidelity, as demonstrated at SIGGRAPH 2018
using over [100GB of production
assets](https://itpeernetwork.intel.com/intel-open-source-libraries-hollywood).

### Support and Contact

HdOSPRay is still in beta, with new features and existing issues being
worked on regularly. USD is also under active development and is often
fast changing. We tag the HdOSPRay releases to match specified USD
releases, but cannot always anticipate all of the changes and resulting
issues for users. Please report any issues you may run into to our
[issue tracker](https://github.com/ospray/hdospray/issues). We always
welcome suggestions and especially pull requests\!

## HdOSPRay Gallery

Our gallery currently contains a limited set of renderings done with
HdOSPRay inside of usdview using publicly available USD datasets. We
hope to grow this gallery as more assets become available. Please let us
know of any great scenes we may be missing, or if you would like to send
us a scene for testing and displaying here\!

Instructions are provided for loading the scenes below.

### Kitchen Set

<center>

<img src="images/hdospray_kitchen_pt.jpg" alt="Pixar Kitchen Set path traced with HdOSPRay in usdview" width=70%>
<br/> Pixar Kitchen Set path traced with HdOSPRay in usdview <br/>

</center>

<br/>

  - Download Kitchen Set asset from
    [Pixar](http://graphics.pixar.com/usd/downloads.html)

  - Run usdview using HdOSPRay using
    
        HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Kitchen_set.usd
    
    <br/>

<center>

<img src="images/usd_gl_thumbnail.jpg" alt="Pixar Kitchen Set GL" width=30%>
<br/> Pixar Kitchen Set rendered with default GL in usdview <br/>

</center>

<br/>

<center>

<img src="images/usd_shadows_thumbnail.jpg" alt="Pixar Kitchen Set shadows" width=30%>
<br/> Pixar Kitchen Set rendered with HdOSPRay and basic shadows in
usdview <br/>

</center>

<br/>

<center>

<img src="images/usd_ao_thumbnail.jpg" alt="Pixar Kitchen Set AO" width=30%>
<br/> Pixar Kitchen Set rendered with HdOSPRay and basic ambient
occlusion in usdview <br/>

</center>

<br/>

<center>

<img src="images/usd_pt_thumbnail.jpg" alt="Pixar Kitchen Set path traced" width=30%>
<br/> Pixar Kitchen Set rendered with HdOSPRay and path tracing in
usdview <br/>

</center>

<br/>

### Teapot

<center>

<img src="images/hdospray_teapot2_thumbnail.jpg" alt="Teapot" width=70%>
<br/> Apple teapot rendered with HdOSPRay in usdview <br/>

</center>

<br/>

  - Download the Apple ARKit teapot from
    [Apple](https://developer.apple.com/arkit/gallery/models/teapot/teapot.usdz)

  - USDZ files are zip files, unzip using platform specific program of
    your choice
    
        unzip teapot.usdz

  - Run usdview using HdOSPRay using
    
        HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Teapot.usdc
    
    <br/>

### Gramophone

<center>

<img src="images/hdospray_gramophone_thumbnail.jpg" alt="Gramophone" width=70%>
<br/> Apple Gramophone rendered with HdOSPRay in usdview <br/>

</center>

<br/>

  - Download the Apple ARKit gramophone from
    [Apple](https://developer.apple.com/arkit/gallery/models/gramophone/gramophone.usdz)

  - USDZ files are zip files, unzip using platform specific program of
    your choice
    
        unzip teapot.usdz

  - Run usdview using HdOSPRay using
    
        HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Gramophone.usdc
    
    <br/>

# Building HdOSPRay

HdOSPRay source is available on GitHub at
[HdOSPRay](http://github.com/ospray/hdospray). The master branch is
typically the most stable branch and contains tagged releases.

Tags are of the form `hdospray-vx.x.x-usdvx.x.x`, with `vx.x.x` being
the release of HdOSPRay and `usdv` being the version of USD it is built
against. This is required due to the often changing internals of hydra
calls in USD.

Currently HdOSPRay is regularly tested on Linux, which is the primary
development target of USD itself. MacOS support in USD is experimental,
but we often test against it. Windows support of USD is also
experimental, but we have not tested HdOSPRay with it, so use at your
own risk.

## Prerequisites

  - [USD v20.05](https://graphics.pixar.com/usd/docs/index.html)
      - USD is primarily tested with Linux, but has experimental support
        for MacOS and Windows. For a full list of USD dependencies, see
        the USD page.
  - [OSPRay 2.1.0](http://www.ospray.org/)
      - We recommend using ospray’s superbuild to build dependencies
        such as embree, ospcommon, and openvkl.
  - [OpenImageIO 1.8.9](https://sites.google.com/site/openimageio/home)
  - CMake 3.1.1+

## Optional Dependencies

  - [OpenImageDenoise](https://github.com/OpenImageDenoise/oidn.git)
      - Open Image Denoise also needs be be enabled in the OSPRay build
  - [Ptex](https://github.com/wdas/ptex)
      - Ptex will need to be enabled in the USD build
      - Can be downloaded and built by the USD `build_usd.py` script
      - [Ptex module](https://github.com/ospray/module_ptex) also needs
        to be enabled in the OSPRay build

## Compiling HdOSPRay on Linux/MacOS

HdOSPRay plugin uses a CMake build system which links in USD and builds
externally from the USD source directory, but configures CMake as if it
were inside the USD repository in order to build the plugin using USD
internals. It must therefore be built against a version of USD that
matches HdOSPRay, which is specified in the versioning tag of HdOSPRay.
To build USD, see the [USD GitHub
site](https://github.com/PixarAnimationStudios/USD). We recommend
following the build scripts provided.

  - Download/clone the git repo for HdOSPRay
    
        $ git clone https://github.com/ospray/hdospray.git

  - Create a build directory and call CMake
    
        $ cd hdospray
        $ mkdir build
        $ cd build
        $ ccmake ..

  - Set `pxr_DIR` to the install directory of USD which contains
    `pxrConfig.cmake`

  - Set required USD options: `usd-imaging` and `openimageio` are
    required for both the USD and HdOSPRay builds

  - Set `ospray_DIR` to the directory containing your
    `osprayConfig.cmake`
    
      - This can be found in the root directory of the distributed
        binaries or if you are building and installing from source it
        can be found in `<install>/lib/cmake/ospray-\*/`

  - Set `embree_DIR` to install directory of Embree. These will be the
    same as `ospray_DIR` if you downloaded the OSPRay binaries

  - Compile and install HdOSPRay
    
        $ make -j install

The plugin should now be in `<usd install
directory>/plugin/usd/hdOSPRay`

## Compiling HdOSPRay on Windows

Windows support of USD is experimental. We have not tested HdOSPRay with
it, so use at your own risk.

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

  - `HDOSPRAY_SAMPLES_PER_FRAME`
    
    Number of samples per pixel

  - `HDOSPRAY_SAMPLES_TO_CONVERGENCE`
    
    Will progressively render frames until this many samples per pixel,
    then stop rendering

  - `HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES`
    
    Number of ambient occlusion samples to compute per pixel. *Does not
    affect path tracer.*

  - `HDOSPRAY_CAMERA_LIGHT_INTENSITY`
    
    Globally modify the intensity of all lights

  - `HDOSPRAY_USE_PATH_TRACING`
    
    Use Monte Carlo path tracer instead of faster Whitted-style renderer

  - `HDOSPRAY_INIT_ARGS`
    
    Specify arguments sent on to OSPRay for initialization. e.g. enable
    MPI

  - `HDOSPRAY_USE_DENOISER`
    
    If built in, enable the denoiser

## Features

  - Denoising using [Open Image Denoise](http://openimagedenoise.org)
  - Distributed multi-node rendering over MPI
  - UVTextures
  - Ptex
  - Triangle meshes
  - Quad meshes
  - Subdivision surfaces
  - Shadows
  - Ambient occlusion
  - Path tracing
  - Physically-based materials
  - Principled shader (similar to Disney BSDF shader)

## TODOs

  - Picking does not work in HdOSPRay. You cannot select objects in the
    viewer yet
  - Custom lights. We are waiting to get actual USD files that specify
    these to test
  - OSL shaders. They are a work in progress in OSPRay
  - Binary releases. We hope to provide these soon

# News, Updates, and Announcements

## June 15, 2020: Version v0.4 now released on GitHub

Update to OSPRay 2.1.0. Update to USD 20.05.

## September 15, 2019: Version v0.3 now released on GitHub

Subdivision surfaces support. GUI options for usdview. Various bug
fixes.

## April 30, 2019: Version v0.2.2 now released on GitHub

Various bug fixes. OSPRay version updated to 1.8.5. CMake targets.
Animation.

## Mar 7, 2019: Version v0.2.1 now released on GitHub

Bug fixes, ptex updates, documentation.

## Feb 28, 2019: Version v0.2.0 now released on GitHub

Initial Beta release version 0.2.0 is now available on the [HdOSPRay
GitHub page](https://github.com/ospray/hdospray/releases/v0.2.0). This
replaces the previous repository and versioning which was on my personal
github account.
