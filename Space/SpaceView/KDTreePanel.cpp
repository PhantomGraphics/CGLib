#include "KDTreePanel.h"

#include "imgui.h"

namespace VKSpace {

void KDTreePanel::onImGui(World& world) {
    ImGui::SliderInt("k", &params_.k, KDTreeDemoParams::kMinK, 20);
    if (ImGui::Button("Run")) run(world);
    if (stats_.lineIndexCount > 0) {
        ImGui::Separator();
        ImGui::Text("Line indices: %d", stats_.lineIndexCount);
    }
}

void KDTreePanel::run(World& world) {
    stats_ = runKDTreeDemo(params_, world.getResult());
    world.markDirty();
}

bool KDTreePanel::setParam(const std::string& name, const std::string& value) {
    return params_.set(name, value);
}

} // namespace VKSpace
