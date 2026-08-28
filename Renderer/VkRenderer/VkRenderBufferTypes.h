#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Vulkan-free CPU-side draw data for VkPointRenderer/VkTriangleRenderer/VkLineRenderer.
// Split out from those renderer classes (which pull in the Vulkan SDK headers via IVkRenderer.h /
// VulkanBuffer.h / VulkanPipeline.h) so that code holding only the data -- not driving the actual
// Vulkan pipeline -- can depend on this lightweight header instead.

namespace Phantom::VKG {

struct VkPointBufferData {
    std::vector<float> positions; ///< Interleaved x,y,z (3 floats per point).
    std::vector<float> colors;    ///< Interleaved r,g,b,a (4 floats per point).
    std::vector<float> sizes;     ///< Point size (1 float per point).
    glm::mat4           projectionMatrix{1.f};
    glm::mat4           modelViewMatrix{1.f};
};

struct VkTriangleBufferData {
    std::vector<float>    positions; ///< Interleaved x,y,z (3 floats per vertex).
    std::vector<float>    colors;    ///< Interleaved r,g,b,a (4 floats per vertex).
    std::vector<uint32_t> indices;   ///< Index list (3 indices per triangle).
    glm::mat4              projectionMatrix{1.f};
    glm::mat4              modelViewMatrix{1.f};
};

struct VkLineBufferData {
    std::vector<float>    positions; ///< Interleaved x,y,z (3 floats per vertex).
    std::vector<float>    colors;    ///< Interleaved r,g,b,a (4 floats per vertex).
    std::vector<uint32_t> indices;   ///< Index list (2 indices per segment).
    glm::mat4              projectionMatrix{1.f};
    glm::mat4              modelViewMatrix{1.f};
};

} // namespace Phantom::VKG
