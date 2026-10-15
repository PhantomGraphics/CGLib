#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "LightEntry.h"

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"

#include <vector>

namespace Phantom::Gltf {

// Manages up to kMaxLights LightEntry values and packs them into a fixed-size,
// std140-compatible GPU buffer on demand.
class LightManager {
public:
    static constexpr int kMaxLights = 8;

    // std140 layout (16-byte aligned members throughout).
    struct LightGpu {
        glm::vec4 positionType;         // xyz = position, w = Type (as float)
        glm::vec4 directionRange;       // xyz = direction, w = range
        glm::vec4 colorIntensity;       // rgb = color, w = intensity
        glm::vec4 spotAngleShadowPad;   // x = outer spotAngle(rad), y = castShadow(0/1), z = inner spotAngle(rad), w unused
    };
    struct LightBufferGpu {
        LightGpu   lights[kMaxLights];
        glm::ivec4 countPad; // x = active light count, y/z/w unused
    };
    static_assert(sizeof(LightGpu) == 64, "LightGpu layout changed -- keep in sync with any consuming shader");
    static_assert(sizeof(LightBufferGpu) == kMaxLights * 64 + 16, "LightBufferGpu layout changed");

    // Returns the new light's id, or -1 if already at kMaxLights capacity.
    int  addLight(const LightEntry& entry);
    void removeLight(int id);
    void setLight(int id, const LightEntry& entry);

    // Entries in insertion order (ids are not exposed here; callers that need to address a
    // specific light again should keep the id returned by addLight()).
    std::vector<LightEntry> lights() const;
    int count() const { return static_cast<int>(entries_.size()); }

    LightBufferGpu buildGpuBuffer() const;

    // Writes buildGpuBuffer() into a mapped UBO (created via
    // VulkanBuffer::createMapped(ctx, sizeof(LightBufferGpu), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)).
    void uploadUBO(Phantom::VKG::VulkanBuffer& ubo) const;

private:
    struct Entry { int id; LightEntry data; };
    std::vector<Entry> entries_;
    int nextId_ = 0;
};

} // namespace Phantom::Gltf
