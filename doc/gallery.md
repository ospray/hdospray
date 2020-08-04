## HdOSPRay Gallery

Our gallery currently contains a limited set of renderings done with HdOSPRay
inside of usdview using publicly available USD datasets. We hope to grow this
gallery as more assets become available. Please let us know of any great scenes
we may be missing, or if you would like to send us a scene for testing and
displaying here!

Instructions are provided for loading some of the scenes below.
<br/>

### Country Kitchen

<p align="center">
<img src="images/hdospray_kitchen2.jpg" alt="Country Kitchen">
<br/>
Converted scene by Jay-Artist <a href="https://creativecommons.org/licenses/by/3.0/">CC</a>, modified by [Bitterli](https://benedikt-bitterli.me/resources/).
<br/>
</p>
<br/>

### Bathroom

<p align="center">
<img src="images/hdospray_bathroom.jpg" alt="Bathroom">
<br/>
Converted scene by Mareck <a href="https://creativecommons.org/publicdomain/zero/1.0/">CC</a>, modified by [Bitterli](https://benedikt-bitterli.me/resources/).
</p>
<br/>
<br/>

### Kitchen Set

<p align="center">
<img src="images/hdospray_kitchen_pt.jpg" alt="Pixar Kitchen Set path traced with HdOSPRay in usdview">
</p>
<br/>
Pixar Kitchen Set path traced with HdOSPRay in usdview
<br/>
</center>
<br/>

- Download Kitchen Set asset from [Pixar](http://graphics.pixar.com/usd/downloads.html)
- Run usdview using HdOSPRay using

    ```
    HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Kitchen_set.usd
    ```
<br/>

<p align="center">
<img src="images/usd_gl_thumbnail.jpg" alt="Pixar Kitchen Set GL" width=30%>
<br/>
Pixar Kitchen Set rendered with default GL in usdview
<br/>
</p>
<br/>

<p align="center">
<img src="images/usd_shadows_thumbnail.jpg" alt="Pixar Kitchen Set shadows" width=30%>
<br/>
Pixar Kitchen Set rendered with HdOSPRay and basic shadows in usdview
<br/>
</p>
<br/>

<p align="center">
<img src="images/usd_ao_thumbnail.jpg" alt="Pixar Kitchen Set AO" width=30%>
<br/>
Pixar Kitchen Set rendered with HdOSPRay and basic ambient 
occlusion in usdview
<br/>
</p>
<br/>


<p align="center">
<img src="images/usd_pt_thumbnail.jpg" alt="Pixar Kitchen Set path traced" width=30%>
<br/>
Pixar Kitchen Set rendered with HdOSPRay and path tracing in usdview
<br/>
</p>
<br/>

### Teapot

<p align="center">
<img src="images/hdospray_teapot2_thumbnail.jpg" alt="Teapot" width=70%>
<br/>
Apple teapot rendered with HdOSPRay in usdview
<br/>
</p>
<br/>

- Download the Apple ARKit teapot from [Apple](https://developer.apple.com/arkit/gallery/models/teapot/teapot.usdz)
- USDZ files are zip files, unzip using platform specific program of your choice

    ```
    unzip teapot.usdz
    ```

- Run usdview using HdOSPRay using

    ```
    HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Teapot.usdc
    ```
<br/>

### Gramophone

<p align="center">
<img src="images/hdospray_gramophone_thumbnail.jpg" alt="Gramophone" width=70%>
<br/>
Apple Gramophone rendered with HdOSPRay in usdview
<br/>
</p>
<br/>

- Download the Apple ARKit gramophone from [Apple](https://developer.apple.com/arkit/gallery/models/gramophone/gramophone.usdz)
- USDZ files are zip files, unzip using platform specific program of your choice

    ```
    unzip teapot.usdz
    ```

- Run usdview using HdOSPRay using

    ```
    HDOSPRAY_USE_PATH_TRACING=1 usdview --renderer OSPRay Gramophone.usdc
    ```
<br/>
