#pragma once

#include <vulkan/vulkan.h>

namespace Phantom::VKG {

// Minimal hook a sub-renderer calls to let an outer renderer time its GPU
// passes with VkQueryPool timestamps. The sub-renderer keeps no Vulkan query
// state of its own -- it only calls gpuMark() at pass boundaries while
// recording into a command buffer the outer renderer owns. All calls are made
// from the render thread, between the outer renderer's beginFrame()/endFrame().
//
// Behaviour-neutral: a sub-renderer with no profiler attached (the default)
// never calls gpuMark(), and an implementation is free to make gpuMark() a
// no-op. Added for docs/todo/PLAN_fluidstudio_fast_rendering.md Phase 0.
class IGpuProfiler {
public:
    virtual ~IGpuProfiler() = default;

    // Records a GPU timestamp into `cmd`. `label` names the interval that just
    // ended (since the previous mark / the frame start). `label` must be a
    // string literal or otherwise outlive the frame.
    virtual void gpuMark(VkCommandBuffer cmd, const char* label) = 0;
};

} // namespace Phantom::VKG
