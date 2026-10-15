# PBR

`PBRCore` contains shared light descriptions, conversions and shadow helpers.
`PBRTest` verifies these CPU components. `PBRView` provides an interactive Vulkan
test scene using the existing `GltfSceneRenderer` and `PBR::toLightEntry`.

From the CGLib root, in a Visual Studio developer shell:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --target PBRView PBRTest
.\build\windows-debug\PBR\PBRTest.exe
.\build\windows-debug\PBR\PBRView.exe
```

The scene requires no model downloads. Columns increase roughness from 0.08 to
1.0; rows increase metallic from 0.0 at the top to 1.0 at the bottom. Controls
change the light type (directional, point, spot), intensity, color, position,
spot cone and camera distance. Directions always point toward the grid center.
Rendering uses direct lighting only; IBL and shadows are disabled.

For an automatic startup check with a screenshot:

```powershell
.\build\windows-debug\PBR\PBRView.exe --smoke-test --screenshot screenshots\pbr.png
```

`--smoke-test` exits after 12 frames. Screenshot options are provided by
`VkAppBase`. On Linux, launch from the executable directory so relative shader
paths resolve. Vulkan and GLFW are required; CPU-only builds and
`CGLIB_BUILD_VIEWERS=OFF` omit `PBRView`.

## Scenario tests

```powershell
.\PBR\PBRView\run_pbr_scenarios.ps1 -Configuration Debug
# Or run one JSON scenario directly:
.\build\windows-debug\PBR\PBRView.exe --run-scenario PBR\PBRView\scenarios\04_render_response.json
```

The runner locates the executable in an enclosing CGLib or Phantom preset build;
`-ExePath` overrides discovery. Each process has a 60-second timeout (override
with `-TimeoutSeconds`). Logs are saved in a new temporary directory printed by
the runner. A failed assertion, initialization error or timeout fails the run.
Scenarios exit automatically; `--smoke-test` cannot be combined with them.
UI controls are disabled while a scenario runs.

The four scenarios cover default scene state, point/spot lighting and control
reset, rejected inputs without state changes, and a GPU pixel readback comparison
that requires increased light intensity to brighten the center sphere.

Commands use space-separated arguments:

| Command | Arguments / result |
| --- | --- |
| `GetState` | JSON `{ok:true,result:{...}}` containing current settings and scene counts |
| `Reset` | Restore default settings |
| `SetLightType` | 0 directional, 1 point, 2 spot |
| `SetIntensity` | 0 to 100 |
| `SetPosition` | x y z, each 1 to 12; light points toward the origin |
| `SetColor` | r g b, each 0 to 1 |
| `SetCone` | 10 to 80 degrees (outer half-angle) |
| `SetDistance` | 15 to 30 |
| `ReadCenterBrightness` | Asynchronous GPU readback; returns center pixel R+G+B (0 to 765) |

Setters return `OK`, or `Error:invalid argument` without changing state.
Unknown commands return `Error:unknown command`.

PBRView uses `shaders/pbr.frag`, based on the GltfViewer PBR shader with
`LightManager` buffer lighting, inverse-square point attenuation and spot cones.
After editing GLSL, run `PBR\PBRView\shaders\compile_shaders.bat` and rebuild
`PBRView`. The compiled SPIR-V is included in the repository.
