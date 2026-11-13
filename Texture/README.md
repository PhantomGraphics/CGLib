# Texture recipe evaluation

CPU-only C++20 texture recipe evaluation. No WPF, Vulkan, PhantomStudio or image inputs are required.

```powershell
cmake -S Phantom/CGLib/Texture -B build/texture -G "Visual Studio 18 2026" -A x64
cmake --build build/texture --config Debug --parallel 1
ctest --test-dir build/texture -C Debug --output-on-failure
build/texture/Debug/TextureCLI.exe --example example.texture.json
build/texture/Debug/TextureCLI.exe example.texture.json example.png
```

The module is also included by CGLib and Phantom's CMake builds. Link `TextureCore` (`CGLib::Texture`) and include `CGLib/Texture/Texture.h`.
Tests use the repository's pinned GoogleTest; for offline configuration set `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` to an existing v1.15.2 source directory.

## Recipe version 1

UTF-8 JSON stores `format_version`, `evaluator_version`, square `resolution` (512/1024/2048/4096; default 1024), `output`, `nodes`, `connections`, and an empty `external_dependencies` array. All node fields are explicit: stable string `id`, `type`, linear straight RGBA `first`/`second`, unsigned 32-bit `seed`, integer `scale` (1..4096), and editor `position` [x,y]. Node IDs and connections are sorted on save. Pixels are never serialized. Save replaces the destination through a temporary file in the same directory. A failed load leaves the caller's recipe intact.

| Type | Required input sockets | Behavior |
|---|---|---|
| solid | none | first color |
| noise | none | seeded integer-hash grid, scale cells across the canvas |
| gradient | none | horizontal first-to-second at pixel centers |
| checker | none | first/second alternating grid, scale cells |
| blend | 0 background, 1 foreground, 2 mask | straight-alpha source-over; mask red multiplies foreground alpha |
| base_color | 0 color | terminal output |

All ordinary sources produce linear RGBA color; Base Color is terminal and cannot be connected as a source. The catalog exposes names and required socket counts. Validation rejects unknown types, duplicate IDs/connections, invalid parameters, missing inputs and cycles across the entire graph (including unused nodes). Graphs are limited to 256 nodes. Evaluation uses a topological order and one pixel's intermediate colors rather than an image per node. Optional cancellation is checked each row and preserves the previous image.

PNG output converts linear RGB to sRGB and keeps alpha linear. Stateless evaluation regenerates pixels from the recipe. The optional preview context retains only derived memory; no disk cache is required. Examples and tests use synthetic data only.

## Paint (recipe/evaluator version 2)

Version 1 remains supported. Version 2 adds `paint`, a zero-input linear RGBA source
with a `strokes` array. Each stroke stores `color` (four floats), `radius` (fraction
of canvas width, 0.00025..1), `strength` and `hardness` (0..1), `erase` (bool), and
`samples` (UV pairs). UVs in -1..2 permit captured pointer motion outside the canvas;
dabs clip rather than wrap. Strokes are limited to 10,000 and samples to 100,000 per
recipe. Paint is forbidden in version 1, as are nonempty strokes on ordinary nodes.

Evaluation interpolates dabs at quarter-radius spacing (minimum half a pixel),
carrying arc length across samples. Hardness controls the linear edge falloff.
Brushes compose in linear straight-alpha; erasing reduces layer alpha and reveals
the background when used as a blend foreground. Re-evaluation at another resolution
replays the normalized samples. Row/dab cancellation preserves the caller's image.
No raster cache is serialized. Existing version 1 serialization is unchanged.

Managed editing/history, Native C ABI/Interop, WPF node editing and 2D paint UI are
implemented under `CGApp/TextureStudio` in the parent workspace. UV/color transform
nodes, image input, persistent disk cache and progress callbacks remain for subsequent
work. PhantomStudio integration is implemented in the parent workspace.

## Packed and incremental previews

`evaluateSrgb8` directly returns packed sRGB RGBA8 or BGRA8 using the PNG encoder's
rounding, avoiding a float-result transfer. `PreviewEvaluator::evaluate` returns BGRA8
and caches the last successful recipe, paint layers and output in memory. Appending
strokes replays their bounds and reevaluates the affected rectangle. Changing previous
strokes, graph semantics or resolution rebuilds. Placement-only changes preserve pixels.
Changed rectangles are backed up and restored on cancellation/failure; both the
caller's result and previous cache remain valid. Destroying the evaluator releases its cache.

Evaluation and painting use up to eight independent row-band workers. Each pixel keeps
the original node/stroke arithmetic order. Tests compare packed bytes against PNGs
from float evaluation and incremental results against complete replay, including erase,
clipping, cancellation, undo-like removal and resolution changes. Recipe/evaluator
versions are unchanged because these paths produce the same pixels.
