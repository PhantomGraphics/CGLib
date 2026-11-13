# Texture phase 1

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

PNG output converts linear RGB to sRGB and keeps alpha linear. There is no persistent image cache in this phase: each evaluation regenerates pixels from the recipe. The example and tests use synthetic data only.

Managed editing/history, ABI/Interop, UV/color transform nodes, image input, stroke/paint storage, progress callbacks, WPF and PhantomStudio integration belong to subsequent phases. This phase's schema deliberately supports only the six implemented node types and rejects unsupported versions/dependencies.
