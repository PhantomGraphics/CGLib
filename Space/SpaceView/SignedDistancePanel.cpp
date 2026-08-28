#include "SignedDistancePanel.h"

#include "imgui.h"
#include "../Space/SignedDistanceCalculator.h"
#include "../../../CGLib/Math/Sphere3d.h"
#include "../../../CGLib/Math/Vector3d.h"

#include <algorithm>
#include <charconv>
#include <limits>

using Phantom::Math::Vector3df;
using Phantom::Math::Sphere3d;
using Phantom::Space::SignedDistanceCalculator;

namespace VKSpace {

void SignedDistancePanel::onImGui(World& world) {
    ImGui::SliderFloat("Sphere Radius", &sphereRadius_, 0.1f, 1.5f);
    ImGui::SliderInt("Samples per axis", &samples_, 5, 50);
    if (ImGui::Button("Run")) run(world);

    if (sampleCount_ > 0) {
        ImGui::Separator();
        ImGui::Text("Sample count: %d", sampleCount_);
        ImGui::Text("SDF range: [%.3f, %.3f]", minSdf_, maxSdf_);
    }
}

void SignedDistancePanel::run(World& world) {
    const Sphere3d<float> sphere(Vector3df(0.f, 0.f, 0.f), sphereRadius_);
    const auto bb = sphere.getBoundingBox();

    auto& res = world.getResult();
    res.clear();

    SignedDistanceCalculator<float> calc;
    const int n = std::max(samples_, 2);
    sampleCount_ = 0;
    minSdf_ = std::numeric_limits<float>::max();
    maxSdf_ = std::numeric_limits<float>::lowest();

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k) {
                const float u = i / static_cast<float>(n);
                const float v = j / static_cast<float>(n);
                const float w = k / static_cast<float>(n);
                const auto pos = bb.getPosition(u, v, w);
                const float sdf = calc.calculate(pos, sphere);
                minSdf_ = std::min(minSdf_, sdf);
                maxSdf_ = std::max(maxSdf_, sdf);
                ++sampleCount_;

                // Map SDF to color: inside (sdf < 0) �� blue, outside �� red
                const float t = std::max(0.f, std::min(1.f, -sdf / sphereRadius_ * 0.5f + 0.5f));
                const glm::vec4 color = {1.f - t, 0.f, t, 1.f};

                res.addPoint(glm::vec3(pos.x, pos.y, pos.z), color, 4.f);
            }

    world.markDirty();
}

bool SignedDistancePanel::setParam(const std::string& name, const std::string& value) {
    if (name == "SphereRadius") {
        float v;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec == std::errc{}) { sphereRadius_ = v; return true; }
        return false;
    } else if (name == "Samples") {
        int v;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec == std::errc{}) { samples_ = v; return true; }
        return false;
    }
    return false;
}

} // namespace VKSpace
