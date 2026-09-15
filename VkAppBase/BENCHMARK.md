# Synchronized rendering time

Set `VKAPP_BENCHMARK_SYNC=1` before starting a VkAppBase app to enable a
per-frame CPU `steady_clock` measurement. The normal path adds no completion wait.
Flags are read once per process. No class layout or virtual interface changes are required.

```powershell
$env:VKAPP_BENCHMARK_SYNC = '1'
$env:VKAPP_BENCHMARK_NO_GUI = '1' # optional: omit ImGui construction/draw for comparison
# Launch the application/scenario here.
```

```sh
VKAPP_BENCHMARK_SYNC=1 VKAPP_BENCHMARK_NO_GUI=1 ./GSView --run-scenario case.json
```

The log emits:

```text
[FrameTiming] frame=85 synchronizedRenderMs=11.047700
```

The interval starts before command-buffer reset/recording and ends after submission
and `vkWaitForFences` reports that this frame completed. It includes pre-render
compute, rendering, composite and any requested GPU screenshot copy. It excludes
swapchain acquisition, prior-frame fence waits, application/scenario updates,
ImGui widget construction, present and timing-log output. With GUI enabled its GPU
draw is included. Export commands executed during application update are excluded.

This is synchronized renderer latency, not total application frame time or FPS.
Serialization changes scheduling and should only be enabled for benchmarking.
Use steady-state samples after warm-up and avoid concurrent GPU workloads.
Keep the renderer's GPU timestamp measurements as a separate field: they measure
GPU work, while this measurement also includes CPU recording/submission and waiting.
Disable both environment flags (or set them to `0`) for normal interactive use.
