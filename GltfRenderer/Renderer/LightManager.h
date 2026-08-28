#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"

#include <vector>

namespace Phantom::Gltf {

// CPU-side description of a single light. Consumed by LightManager::buildGpuBuffer() into
// LightGpu; not yet sampled by GltfSceneRenderer's PBR shader (which still uses the single
// lightPos/lightColor pair in GlobalUBO) -- LightManager exists as forward-looking scaffolding
// for Universe's multi-light UniverseRenderer (see docs/todo/PLAN_universe_app.md Phase E).
struct LightEntry {
    enum class Type { Directional = 0, Point = 1, Spot = 2 };

    Type      type        = Type::Directional;
    glm::vec3 position     = { 0.f, 10.f, 0.f };
    glm::vec3 direction    = { 0.f, -1.f, 0.f };
    glm::vec3 color        = { 1.f, 1.f, 1.f };
    float     intensity    = 1.0f;
    float     range        = 20.0f;  // Point/Spot only
    float     spotAngleDeg = 30.0f;  // Spot only
    bool      castShadow   = false;
};

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
        glm::vec4 spotAngleShadowPad;   // x = spotAngle(rad), y = castShadow(0/1), z/w unused
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
