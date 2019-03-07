Documentation
=============

## Running
-Once built, the plugin code should be located in your usd install directory
-under `plugin/usd/HdOSPRay`.

-run `usdview <scenefile>` and select `view->Hydra` and then `Renderer->OSPRay`.


OSPRay can be set to the default renderer by setting 
- `export HD_DEFAULT_RENDERER=OSPRay`
- Or by specifying `--renderer OSPRay` as a commandline argument to usdview.


### Environment variables


- HDOSPRAY_SAMPLES_PER_FRAME

   number of samples per pixel

- HDOSPRAY_SAMPLES_TO_CONVERGENCE

   will progressively render frames until this many samples per pixel, then stop rendering

- HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES

   number of ambient occlusion samples to compute per pixel.  *Does not affect path tracer.*

- HDOSPRAY_CAMERA_LIGHT_INTENSITY

   globally modify the intensity of all lights

- HDOSPRAY_USE_PATH_TRACING

   use monte carlo path tracer instead of faster Whitted-style renderer

- HDOSPRAY_INIT_ARGS

   specify arguments sent on to OSPRay for initialization.  e.g. enable MPI

- HDOSPRAY_USE_DENOISER

   if built in, enable the denoiser

## Features
- Denoising using [Open Image Denoise](http://openimagedenoise.org)
- Distributed multi-node rendering over MPI
- UVTextures
- Ptex 
- Triangle meshes
- Quad meshes
- Shadows
- Ambient Occlusion
- Path tracing
- Physically based materials
- Principled shader (similar to Disney BSDF shader)

## Missing Features
- Picking does not work in HdOSPRay.  You cannot select objects in the viewer yet.
- Custom lights.  We are waiting to get actual USD files that specify these to test.
- Custom GUI widgets.  We will add the env vars as python widgets.
- Simple lights.  These are GL specific in USD and are the lights modified in the main menu.
- Subdivision surfaces.  This was put into OSPRay and we will be working on putting this in a future HDOSPRay release.
- OSL shaders.  They are a work in progress in OSPRay.
- Binary releases.  We hope to provide these soon.
