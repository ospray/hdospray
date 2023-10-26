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

- `HDOSPRAY_DEVICE`

   cpu (default) or gpu device.

- `HDOSPRAY_USE_DENOISER`

   If built in, enable the denoiser

- `HDOSPRAY_SAMPLES_PER_FRAME`

   Number of samples per pixel.

- `HDOSPRAY_SAMPLES_TO_CONVERGENCE`

   Will progressively render frames until this many samples per pixel, then stop rendering.

- `HDOSPRAY_INTERACTIVE_TARGET_FPS`

   Set interactive scaling to match target fps when interacting.  0 Disables interactive scaling.

-   `HDOSPRAY_LIGTH_SAMPLES`

   Number of light samples at every path intersection. A value of -1 leads to sampling all light
   sources in the scene. *Does not affect scivis renderer.*

- `HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES`

   Number of ambient occlusion samples to compute per pixel.  *Does not affect path tracer.*

- `HDOSPRAY_CAMERA_LIGHT_INTENSITY`

   Globally modify the intensity of all lights.

- `HDOSPRAY_USE_PATH_TRACING`

   Use Monte Carlo path tracer instead of faster Whitted-style renderer.

- `HDOSPRAY_MAX_PATH_DEPTH`

  Maximum ray depth.  Will affect the number of diffuse bounces
  as well as transparency.

- `HDOSPRAY_USE_SIMPLE_MATERIAL`

  Use a simple diffuse + phone material instead of principled material.

- `HDOSPRAY_INIT_ARGS`

   Specify arguments sent on to OSPRay for initialization.  e.g. enable MPI.

- `HDOSPRAY_PIXELFILTER_TYPE`

   The type of pixel filter used by OSPRay: 0 (point), 1 (box), 2 (gauss), 3 (mitchell), and 4 (blackmanHarris).

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
