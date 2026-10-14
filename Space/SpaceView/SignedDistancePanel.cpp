#include "SignedDistancePanel.h"

#include "imgui.h"

namespace Phantom::VKSpace {

void SignedDistancePanel::onImGui(World& world) {
    ImGui::SliderFloat("Sphere Radius", &params_.sphereRadius, 0.1f, 1.5f);
    ImGui::SliderInt("Samples per axis", &params_.samples, 5, 50);
    if (ImGui::Button("Run")) run(world);
    if (stats_.sampleCount > 0) {
        ImGui::Separator();
        ImGui::Text("Sample count: %d", stats_.sampleCount);
        ImGui::Text("SDF range: [%.3f, %.3f]", stats_.minSdf, stats_.maxSdf);
    }
}

void SignedDistancePanel::run(World& world) {
    stats_ = runSignedDistanceDemo(params_, world.getResult());
    world.markDirty();
}

bool SignedDistancePanel::setParam(const std::string& name, const std::string& value) {
    return params_.set(name, value);
}

} // namespace VKSpace
