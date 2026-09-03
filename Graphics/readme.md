# Graphics Module — File Layout

The module uses the `Phantom::Graphics` namespace and provides cameras, color
spaces, and image I/O. It depends on `Math`. See
[`../docs/module-reference.md`](../docs/module-reference.md) for API details.

```mermaid
flowchart TB
    A["GraphicsCore\n(static library)"]

    subgraph HC[Camera]
        HC1[Camera.h]
    end

    subgraph HColor[Color]
        HColor1[ColorRGB.h / ColorRGBA.h / ColorHSV.h]
        HColor2[ColorConverter.h]
        HColor3[ColorTable.h / ColorMap.h]
    end

    subgraph HI[Image I/O]
        HI1[Image.h]
        HI2[ImageFileReader.h]
        HI3[ImageFileWriter.h]
    end

    A --> HC
    A --> HColor
    A --> HI
```

- `Camera` switches between perspective and orthographic projection and
  returns GLM-based matrices from `getViewMatrix()`, `getProjectionMatrix()`,
  and `getModelMatrix()`.
- `ImageFileReader` and `ImageFileWriter` use STB for 8-bit RGBA and HDR
  floating-point images.
- `ColorMap` provides linear value-to-color mapping with min/max normalization
  and color-table interpolation for scientific visualization.
