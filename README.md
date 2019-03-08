HdOSPRay
========

OSPRay interactive rendering plugin for USD Hydra
-------------------------------------------------

<center>
<img src="images/hdospray_kitchen_pt.jpg" style="padding: 0px 0px 0px 0px; box-shadow: 0px 0px 24px rgba(0,0,0,0.4);">
<p>
<em>Pixar's Kitchen asset rendered with OSPRay's path tracer at interactive rates.</em>
</p>
</center>
This is release v0.2.0 of HdOSPRay.
Visit [HdOSPRay on github](https://github.com/ospray/hdospray) for more information.

HdOSPRay Overview
=================

HdOSPRay is an open source plugin for Pixar's USD to extend USD's Hydra rendering framework with [Intel®
OSPRay](https://www.ospray.org). HdOSPRay enables interactive scene preview by
utilizing OSPRay's high quality renderers and
[Intel® Open Image Denoise](http://openimagedenoise.org), and is released under the permissive Apache 2.0 license.
As part of the [Intel® Rendering
Framework](https://software.intel.com/en-us/rendering-framework), OSPRay is
highly-optimized for Intel® CPU architectures ranging from laptops to large-scale
distributed HPC systems. HdOSPRay leverages the Intel® Rendering
Framework to deliver interactive rendering for large-scale models at high levels of fidelity, as demonstrated at SIGGRAPH 2018 using over [100GB of production
assets](https://itpeernetwork.intel.com/intel-open-source-libraries-hollywood).

Support and Contact
-------------------

HdOSPRay is still in Beta with new features and existing issues being worked on regularly. USD is also under active development and is often fast changing. We tag the HdOSPRay releases to match specified USD releases,
but cannot always anticipate all of the changes and resulting issues for users. Please
report any issues you may run into to our [issue tracker](https://github.com/ospray/hdospray/issues). We always welcome suggestions and especially pull requests!

HdOSPRay Gallery
================

Our gallery currently contains a limited set of renderings done with HdOSPRay
inside of usdview using publicly available USD datasets. We hope to grow this
gallery as more assets become available. Please let us know of any great scenes
we may be missing, or you would like to send us for testing and display here!

Instructions are provided for loading the scenes below.

Kitchen\_Set
------------

<center>
<img src="images/hdospray_kitchen_pt.jpg" alt="Pixar Kitchen Set path traced with HdOSPRay in usdview" width=70%>
<br/>
Pixar Kitchen Set path traced with HdOSPRay in usdview
<br/>
</center>
<br/>

-   Download Kitchen set asset from [pixar](http://graphics.pixar.com/usd/downloads.html)
-   Run usdview using HdOSPRay using

<!-- -->

    HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Kitchen_set.usd

<br/>

<center>
<img src="images/usd_gl_thumbnail.jpg" alt="Pixar Kitchen Set GL" width=30%>
<br/>
Pixar Kitchen Set rendered with default GL in usdview
<br/>
</center>
<br/>

<center>
<img src="images/usd_shadows_thumbnail.jpg" alt="Pixar Kitchen Set shadows" width=30%>
<br/>
Pixar Kitchen Set rendered with HdOSPRay and basic shadows in usdview
<br/>
</center>
<br/>

<center>
<img src="images/usd_ao_thumbnail.jpg" alt="Pixar Kitchen Set AO" width=30%>
<br/>
Pixar Kitchen Set rendered with HdOSPRay and basic ambient
occlusion in usdview
<br/>
</center>
<br/>

<center>
<img src="images/usd_pt_thumbnail.jpg" alt="Pixar Kitchen Set path traced" width=30%>
<br/>
Pixar Kitchen Set rendered with HdOSPRay and path tracing in usdview
<br/>
</center>
<br/>

Teapot
------

<center>
<img src="images/hdospray_teapot2_thumbnail.jpg" alt="Teapot" width=70%>
<br/>
Apple teapot rendered with HdOSPRay in usdview
<br/>
</center>
<br/>

-   Download the Apple ARKit teapot from [apple](https://developer.apple.com/arkit/gallery/models/teapot/teapot.usdz).
-   USDZ files are zip files, unzip using platform specific program of your choice.

<!-- -->

    unzip teapot.usdz

-   Run usdview using HdOSPRay using

<!-- -->

    HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Teapot.usdc

<br/>

Gramophone
----------

<center>
<img src="images/hdospray_gramophone_thumbnail.jpg" alt="Gramophone" width=70%>
<br/>
Apple Gramophone rendered with HdOSPRay in usdview
<br/>
</center>
<br/>

-   Download the Apple ARKit gramophone from [apple](https://developer.apple.com/arkit/gallery/models/gramophone/gramophone.usdz)
-   USDZ files are zip files, unzip using platform specific program of your choice.

<!-- -->

    unzip teapot.usdz

-   Run usdview using HdOSPRay using

<!-- -->

    HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Gramophone.usdc

<br/>

Building HdOSPRay
=================

HdOSPRay source is available on GitHub at
[HdOSPRay](http://github.com/ospray/hdospray). The master branch
is typically the most stable branch and contains tagged releases.
Tags are of the form hdospray-vx.x.x-usdvx.x.x, with vx.x.x being
the release of HdOSPRay and usdv being the version of USD it is built
against. This is required due to the often changing internals of hydra
calls in USD. Currently HdOSPRay is regularly tested on linux, which is
the primary development target of USD itself. MacOS support in USD is
experimental, but we often test against it. Windows support of USD is
experimental, but we have not tested HdOSPRay with it so it is use
at your own risk.

Prerequisites
-------------

-   [USD v19.03](https://graphics.pixar.com/usd/docs/index.html)
    -   USD is primarily tested with Linux, but has experimental support for MacOS and Windows.
        For a full list of USD dependencies, see the USD page.
    -   The plugin requires a minimum of PXR\_BUILD\_IMAGING and
        PXR\_BUILD\_OPENIMAGEIO\_PLUGIN to be set to ON for USD.
-   [OSPRay 1.8.x](http://www.ospray.org/)
-   [Embree 3.2.x](https://embree.github.io/)
-   CMake 3.1.1+

Optional Dependancies
=====================

-   [OpenImageDenoise](https://github.com/OpenImageDenoise/oidn.git)
    - Open Image Denoise also needs be be enabled in the OSPRay build
-   [Ptex](https://github.com/wdas/ptex)
    - Ptex will need to be enabled in the USD build
    - Can be downloaded and built by the USD build\_usd.py script
    - [Ptex module](https://github.com/ospray/module_ptex) also needs to be
    enabled in the OSPRay build

Compiling HdOSPRay on Linux/macOS
---------------------------------

HdOSPRay plugin uses a cmake build system which links in USD and builds
externally from the USD source directory, but configures cmake as if it were
inside the USD repository in order to build the plugin using USD internals. It
must therefore be built against a version of USD that matches HdOSPRay, which is
specified in the versioning tag of HdOSPRay. To build USD, see the [USD GitHub
site](https://github.com/PixarAnimationStudios/USD). We recommend following the
build scripts provided.

-   download/clone the git repo for HdOSPRay

<!-- -->

    git clone https://github.com/ospray/hdospray.git

-   create a build directory and call cmake

<!-- -->

    cd hdospray
    mkdir build
    cd build
    ccmake ..

-   set pxr\_DIR to the install directory of USD which contains pxrConfig.cmake
-   set required USD options: `usd-imaging` and `openimageio` are required for
    both the USD and HdOSPRay builds.
-   set ospray\_DIR to the directory containing your osprayConfig.cmake.
    -   This can be found in the root directory of the distributed binaries or
        if you are building and installing from source it can be found in
        `<install>/lib/cmake/ospray-\*/`
-   set embree\_DIR to install directory of embree. These will be the same as
    ospray\_DIR if you downloaded the ospray binaries.

<!-- -->

    make -j install

-   the plugin should now be in \<usd install directory\>/plugin/usd/hdOSPRay

Compiling HdOSPRay on Windows
-----------------------------

Windows support of USD is
experimental. We have not tested HdOSPRay with it, so use
at your own risk.

Documentation
=============

Running
-------

-Once built, the plugin code should be located in your usd install directory
-under `plugin/usd/HdOSPRay`.

-run `usdview <scenefile>` and select `view->Hydra` and then `Renderer->OSPRay`.

OSPRay can be set to the default renderer by setting
- `export HD_DEFAULT_RENDERER=OSPRay`
- Or by specifying `--renderer OSPRay` as a commandline argument to usdview.

### Environment variables

-   HDOSPRAY\_SAMPLES\_PER\_FRAME

    number of samples per pixel

-   HDOSPRAY\_SAMPLES\_TO\_CONVERGENCE

    will progressively render frames until this many samples per pixel, then stop rendering

-   HDOSPRAY\_AMBIENT\_OCCLUSION\_SAMPLES

    number of ambient occlusion samples to compute per pixel. *Does not affect path tracer.*

-   HDOSPRAY\_CAMERA\_LIGHT\_INTENSITY

    globally modify the intensity of all lights

-   HDOSPRAY\_USE\_PATH\_TRACING

    use monte carlo path tracer instead of faster Whitted-style renderer

-   HDOSPRAY\_INIT\_ARGS

    specify arguments sent on to OSPRay for initialization. e.g. enable MPI

-   HDOSPRAY\_USE\_DENOISER

    if built in, enable the denoiser

Features
--------

-   Denoising using [Open Image Denoise](http://openimagedenoise.org)
-   Distributed multi-node rendering over MPI
-   UVTextures
-   Ptex
-   Triangle meshes
-   Quad meshes
-   Shadows
-   Ambient Occlusion
-   Path tracing
-   Physically based materials
-   Principled shader (similar to Disney BSDF shader)

Missing Features
----------------

-   Picking does not work in HdOSPRay. You cannot select objects in the viewer yet.
-   Custom lights. We are waiting to get actual USD files that specify these to test.
-   Custom GUI widgets. We will add the env vars as python widgets.
-   Simple lights. These are GL specific in USD and are the lights modified in the main menu.
-   Subdivision surfaces. This was put into OSPRay and we will be working on putting this in a future HDOSPRay release.
-   OSL shaders. They are a work in progress in OSPRay.
-   Binary releases. We hope to provide these soon.
