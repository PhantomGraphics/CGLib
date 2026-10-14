#include "OctreePanel.h"

#include "imgui.h"

namespace Phantom::VKSpace {

void OctreePanel::onImGui(World& world) {
    ImGui::SliderInt("Max Depth", &params_.maxDepth, OctreeDemoParams::kMinDepth, OctreeDemoParams::kMaxDepth);
    if (ImGui::Button("Run")) run(world);
    if (stats_.nodeCount > 0) {
        ImGui::Separator();
        ImGui::Text("Node count: %d", stats_.nodeCount);
    }
}

void OctreePanel::run(World& world) {
    stats_ = runOctreeDemo(params_, world.getResult());
    world.markDirty();
}

bool OctreePanel::setParam(const std::string& name, const std::string& value) {
    return params_.set(name, value);
}

} // namespace VKSpace
