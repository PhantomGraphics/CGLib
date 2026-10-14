#include "SpaceHashPanel.h"

#include "imgui.h"

namespace Phantom::VKSpace {

void SpaceHashPanel::onImGui(World& world) {
    ImGui::SliderFloat("Search Radius", &params_.searchRadius, 0.05f, 1.0f);
    if (ImGui::Button("Run")) run(world);
    if (stats_.totalNeighbors > 0) {
        ImGui::Separator();
        ImGui::Text("Neighbor references: %d", stats_.totalNeighbors);
    }
}

void SpaceHashPanel::run(World& world) {
    stats_ = runSpaceHashDemo(params_, world.getResult());
    world.markDirty();
}

bool SpaceHashPanel::setParam(const std::string& name, const std::string& value) {
    return params_.set(name, value);
}

} // namespace VKSpace
