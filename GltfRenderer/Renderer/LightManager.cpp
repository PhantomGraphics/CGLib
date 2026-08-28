#include "LightManager.h"

#include <algorithm>

namespace Phantom::Gltf {

int LightManager::addLight(const LightEntry& entry)
{
    if (static_cast<int>(entries_.size()) >= kMaxLights)
        return -1;

    int id = nextId_++;
    entries_.push_back({ id, entry });
    return id;
}

void LightManager::removeLight(int id)
{
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [id](const Entry& e) { return e.id == id; }),
        entries_.end());
}

void LightManager::setLight(int id, const LightEntry& entry)
{
    for (auto& e : entries_) {
        if (e.id == id) {
            e.data = entry;
            return;
        }
    }
}

std::vector<LightEntry> LightManager::lights() const
{
    std::vector<LightEntry> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.data);
    return out;
}

LightManager::LightBufferGpu LightManager::buildGpuBuffer() const
{
    LightBufferGpu buf{};
    int n = std::min<int>(static_cast<int>(entries_.size()), kMaxLights);
    for (int i = 0; i < n; ++i) {
        const LightEntry& e = entries_[i].data;
        LightGpu& g = buf.lights[i];
        g.positionType       = glm::vec4(e.position, static_cast<float>(static_cast<int>(e.type)));
        g.directionRange     = glm::vec4(e.direction, e.range);
        g.colorIntensity     = glm::vec4(e.color, e.intensity);
        g.spotAngleShadowPad = glm::vec4(glm::radians(e.spotAngleDeg), e.castShadow ? 1.f : 0.f, 0.f, 0.f);
    }
    buf.countPad = glm::ivec4(n, 0, 0, 0);
    return buf;
}

void LightManager::uploadUBO(Phantom::VKG::VulkanBuffer& ubo) const
{
    LightBufferGpu buf = buildGpuBuffer();
    ubo.write(&buf, sizeof(buf));
}

} // namespace Phantom::Gltf
