# Documentation

## Running

Once built, the plugin code should be located in your `usd` install directory under `plugin/usd/HdOSPRay`.
Run `usdview <scenefile>` and select `view->Hydra` and then `Renderer->OSPRay`.

OSPRay can be set to the default renderer by either

- Setting the `HD_DEFAULT_RENDERER` environment variable

    ```
    $ export HD_DEFAULT_RENDERER=OSPRay
    ```

- Specifying `--renderer OSPRay` as a command line argument to usdview


### Environment variables

Most of these options are also exposed through the usdview GUI under
Hydra Settings.


- `HDOSPRAY_SAMPLES_PER_FRAME`

   Number of samples per pixel

- `HDOSPRAY_SAMPLES_TO_CONVERGENCE`

   Will progressively render frames until this many samples per pixel, then stop rendering

- `HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES`

   Number of ambient occlusion samples to compute per pixel.  *Does not affect path tracer.*

- `HDOSPRAY_CAMERA_LIGHT_INTENSITY`

   Globally modify the intensity of all lights

- `HDOSPRAY_USE_PATH_TRACING`

   Use Monte Carlo path tracer instead of faster Whitted-style renderer

- `HDOSPRAY_INIT_ARGS`

   Specify arguments sent on to OSPRay for initialization.  e.g. enable MPI

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

- Picking does not work in HdOSPRay.  You cannot select objects in the viewer yet
- Custom lights.  We are waiting to get actual USD files that specify these to test
- OSL shaders.  They are a work in progress in OSPRay
- Binary releases.  We hope to provide these soon
