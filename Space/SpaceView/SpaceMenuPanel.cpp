#include "SpaceMenuPanel.h"

#include "Renderer.h"
#include "World.h"

#include "imgui.h"

#include "../../../CGLib/VkAppBase/ScenarioRunner/ViewShell.h"

namespace VKSpace {

void SpaceMenuPanel::init(World* world, Renderer* renderer) {
    world_ = world;
    renderer_ = renderer;
    setActive(AlgoType::SpaceHash);
}

void SpaceMenuPanel::onImGuiMenuBar() {
    if (!ImGui::BeginMenu("Space")) return;

    for (AlgoType t : { AlgoType::SpaceHash, AlgoType::CompactSpaceHash,
                        AlgoType::KDTree, AlgoType::Octree, AlgoType::SignedDistance }) {
        if (ImGui::MenuItem(algoName(t), nullptr, t == activeType_, !locked_)) {
            if (submit_) submit_(std::string("SetAlgorithm:") + algoName(t));
            if (shell_) shell_->setPanelVisible("Control", true);
        }
    }

    ImGui::EndMenu();
}

void SpaceMenuPanel::onImGui() {
    if (!shell_ || !shell_->beginPanel("Control")) return;
    ImGui::BeginDisabled(locked_);

    const char* current = algoName(activeType_);
    if (ImGui::BeginCombo("Algorithm", current)) {
        for (AlgoType t : { AlgoType::SpaceHash, AlgoType::CompactSpaceHash,
                            AlgoType::KDTree, AlgoType::Octree, AlgoType::SignedDistance }) {
            const bool selected = (t == activeType_);
            if (ImGui::Selectable(algoName(t), selected) && submit_)
                submit_(std::string("SetAlgorithm:") + algoName(t));
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    if (activeView_ && world_) {
        activeView_->onImGui(*world_);
    }

    ImGui::Separator();
    if (renderer_) {
        if (ImGui::Button("Reset Camera") && submit_)
            submit_("ResetCamera");
    }

    ImGui::EndDisabled();
    shell_->endPanel();
}

const char* SpaceMenuPanel::algoName(AlgoType t) {
    switch (t) {
    case AlgoType::SpaceHash: return "SpaceHash";
    case AlgoType::CompactSpaceHash: return "CompactSpaceHash";
    case AlgoType::KDTree: return "KDTree";
    case AlgoType::Octree: return "Octree";
    case AlgoType::SignedDistance: return "SignedDistance";
    default: return "Unknown";
    }
}

IAlgorithmView* SpaceMenuPanel::viewOf(AlgoType t) {
    switch (t) {
    case AlgoType::SpaceHash: return &spaceHashView_;
    case AlgoType::CompactSpaceHash: return &compactHashView_;
    case AlgoType::KDTree: return &kdTreeView_;
    case AlgoType::Octree: return &octreeView_;
    case AlgoType::SignedDistance: return &signedDistanceView_;
    default: return nullptr;
    }
}

void SpaceMenuPanel::setActive(AlgoType t) {
    activeType_ = t;
    activeView_ = viewOf(t);
}

void SpaceMenuPanel::setActiveByName(const std::string& name) {
    for (AlgoType t : { AlgoType::SpaceHash, AlgoType::CompactSpaceHash,
                        AlgoType::KDTree, AlgoType::Octree, AlgoType::SignedDistance }) {
        if (name == algoName(t)) { setActive(t); return; }
    }
}

void SpaceMenuPanel::runActive(World& world) {
    if (activeView_) activeView_->run(world);
}

bool SpaceMenuPanel::setActiveParam(const std::string& name, const std::string& value) {
    if (activeView_) return activeView_->setParam(name, value);
    return false;
}

} // namespace VKSpace
