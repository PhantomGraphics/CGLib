# Water surface

Finite horizontal rectangular height field, Vulkan compute updates and a displaced
grid renderer. This is a visual wave model, not a shallow-water flow solver.
No Physics dependency, external models, underwater camera or breaking waves.

## Build and demo

From the CGLib root in a VS2026 developer shell:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --target WaterSurfaceView WaterSurfaceTest
ctest --preset windows-debug -R WaterSurfaceReference
.\build\windows-debug\Effects\WaterSurfaceView.exe
.\Effects\WaterSurface\run_water_scenarios.ps1 -Configuration Debug
```

Standalone configuration: `cmake -S Effects -B build/Effects`. The standard
`CGLIB_BUILD_TESTING` and `CGLIB_BUILD_VIEWERS` switches apply. GPU targets require
Vulkan; the viewer also requires GLFW. CPU-only top-level builds omit Effects.
The component remains build-tree-only, with no installed-package export.

Left click injects a ripple; right drag orbits the camera. The panel provides
pause, single step, reset, ripple controls, material controls and simulation
settings. Wave speed, damping, grid and domain edits take effect on Apply,
which resets the simulation. Coordinates cover `[0,width] x [0,depth]` in XZ;
height and impulse velocity use the same world units and seconds.

GLSL sources and precompiled SPIR-V are under `shaders/`. After edits run
`shaders/compile_shaders.bat` using Vulkan SDK glslc, then rebuild the viewer.
It copies the shaders under `shaders/water` next to the executable. On Linux,
run from that executable directory (the current SpvResolver contract).

## Integration contract

1. Create `WaterSurfaceSimulator` with config and compute SPIR-V; create
   `WaterSurfaceRenderer` with matching config, render pass, frame count and
   vertex/fragment SPIR-V. Both return false on creation failure; destroy safely
   after GPU work completes. Their owners must keep the Vulkan context alive.
2. After the current frame fence, record simulator updates in `onPreRender`,
   outside render passes. Record and submit each update exactly once, in order,
   on the graphics queue. Do not use a separate compute queue without additional
   synchronization. Impulses are queued (maximum 256), then applied once at the
   next update, including when paused; reset clears queued impulses.
3. Produce a planar-reflection color image with a mirrored camera and a scene
   color/depth image from the regular camera with geometry clipped below the
   mean water plane. Reflection includes geometry above that plane. Neither
   pass includes water. The demo ray-casts a checker floor and three boxes in
   these passes, and draws the complete scene before water in the final pass.
4. Supply non-owning `WaterSurfaceImages`: single-sample color textures in
   `SHADER_READ_ONLY_OPTIMAL` and sampled depth in
   `DEPTH_STENCIL_READ_ONLY_OPTIMAL`, with valid samplers. Supply view-projection,
   inverse view-projection and reflection view-projection matching those images.
   Vulkan depth is `[0,1]`; positive viewport height and a Y-flipped projection
   are used in the demo. Call renderer `update` after simulation recording so
   its descriptor refers to the current ping-pong buffer, then `recordDraw`
   inside the final render pass. All images must be valid even with effects off.
5. Use per-frame input targets or otherwise wait before overwriting them. The
   renderer maintains per-frame uniforms/descriptors; the caller waits that
   frame's fence before updating them. On resize, wait for in-flight work,
   recreate image resources/pipelines as needed and refresh descriptors.

Five-point differences update velocity and height with semi-implicit Euler.
Outer cells are fixed at zero. Exponential damping is per second. Time is
accumulated at min(1/120 s, 0.9 / (c sqrt(1/dx²+1/dz²))); a frame runs at most
16 steps and drops excess time. Pause accumulates no new time; single step
advances once. Normals are derived from the final height buffer in the vertex
shader. Fresnel blends planar reflection with distorted scene refraction and
vertical-depth RGB absorption; UVs clamp to the image edge. This deliberately
does not implement physically exact ray-traced refraction or offscreen scenery.

## Validation

`WaterSurfaceTest` checks rest/reset, invalid configuration, symmetry, fixed
boundaries, frame partition, pause, CFL stability, linear superposition and
energy decay. The viewer scenario command `verify` compares a 23x17 GPU field
against the CPU oracle over 60 frames (two impulses, pause and reset), including
height and velocity at every cell, with absolute tolerance 1e-5. GPU readback
is confined to this explicit diagnostic. Runtime simulation does no readback.
`WaterSurfaceGpuTest` registers the same numerical comparison with CTest using
a hidden window, and explicitly skips when a Vulkan window/device is unavailable.
The interactive viewer scenarios additionally exercise validation-enabled drawing,
two frames in flight, material switches and resize.

Scenario commands: `status`, `verify`, `reset`, `pause 0|1`, `step`, `wait`,
`impulse x z radius velocity`, `resize width height`, `reflection 0|1`,
`refraction 0|1`, `screenshot path`. Scenarios use fixed 1/60 s frame timing and
return nonzero on failure. Vulkan validation is enabled by VkAppBase. Capture
with `--screenshot path --screenshot-frame N`; the viewer keeps running unless
the scenario completes or the window closes.
