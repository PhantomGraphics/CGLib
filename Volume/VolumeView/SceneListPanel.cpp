#include "SceneListPanel.h"

#include "imgui.h"

#include <algorithm>
#include <string>

namespace VkVolumeView {

void SceneListPanel::init(World* world, int* pActiveSceneId, int* pActiveDenseSceneId,
                                 std::function<void()> onWorldChanged,
                                 std::function<void()> onSceneSelectionChanged)
{
    world_                   = world;
    pSparseId_               = pActiveSceneId;
    pDenseId_                = pActiveDenseSceneId;
    onWorldChanged_          = std::move(onWorldChanged);
    onSceneSelectionChanged_ = std::move(onSceneSelectionChanged);
}

void SceneListPanel::onImGui() {
    if (!world_ || !pSparseId_ || !pDenseId_) return;

    if (!ImGui::BeginTabBar("SceneTabs")) return;

    if (ImGui::BeginTabItem("Sparse Volumes")) {
        ImGui::Text("Scenes (%zu)", world_->getScenes().size());

        const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
        const float listHeight = std::max(itemHeight * 4.f, 80.f);
        ImGui::BeginChild("SceneListSparse", ImVec2(0.f, listHeight), true);

        int removeId = -1;
        for (const auto& scene : world_->getScenes()) {
            const int  id      = scene->getId();
            bool       visible = scene->isVisible();
            const bool active  = (*pSparseId_ == id);

            ImGui::PushID(id);

            if (ImGui::Checkbox("##vis", &visible)) {
                scene->setVisible(visible);
                if (onWorldChanged_) onWorldChanged_();
            }
            ImGui::SameLine();

            const int voxels = scene->getShape()
                ? scene->getShape()->getActiveVoxelCount() : 0;
            const std::string label =
                scene->getName() + " (" + std::to_string(voxels) + " vx)";

            if (ImGui::Selectable(label.c_str(), active)) {
                *pSparseId_ = id;
                if (onSceneSelectionChanged_) onSceneSelectionChanged_();
                else if (onWorldChanged_)     onWorldChanged_();
            }
            ImGui::SameLine();

            if (ImGui::SmallButton("X")) removeId = id;

            ImGui::PopID();
        }
        ImGui::EndChild();

        if (removeId >= 0) {
            if (*pSparseId_ == removeId) *pSparseId_ = -1;
            world_->removeScene(removeId);
            if (onWorldChanged_) onWorldChanged_();
            if (*pSparseId_ < 0 && !world_->getScenes().empty())
                *pSparseId_ = world_->getScenes().front()->getId();
        }

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Dense Volumes")) {
        ImGui::Text("Dense (%zu)", world_->getDenseScenes().size());

        const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
        const float listHeight = std::max(itemHeight * 4.f, 80.f);
        ImGui::BeginChild("SceneListDense", ImVec2(0.f, listHeight), true);

        int removeDenseId = -1;
        for (const auto& scene : world_->getDenseScenes()) {
            const int id = scene->getId();
            bool visible = scene->isVisible();
            const bool active = (*pDenseId_ == id);

            ImGui::PushID(100000 + id);
            if (ImGui::Checkbox("##dvis", &visible)) {
                scene->setVisible(visible);
                if (onWorldChanged_) onWorldChanged_();
            }
            ImGui::SameLine();

            const int voxels = scene->getVolume()
                ? static_cast<int>(scene->getVolume()->getResolutions()[0] *
                                   scene->getVolume()->getResolutions()[1] *
                                   scene->getVolume()->getResolutions()[2])
                : 0;
            const std::string label =
                scene->getName() + " (" + std::to_string(voxels) + " cells)";

            if (ImGui::Selectable(label.c_str(), active)) {
                *pDenseId_ = id;
                if (onSceneSelectionChanged_) onSceneSelectionChanged_();
                else if (onWorldChanged_)     onWorldChanged_();
            }
            ImGui::SameLine();

            if (ImGui::SmallButton("X")) removeDenseId = id;

            ImGui::PopID();
        }
        ImGui::EndChild();

        if (removeDenseId >= 0) {
            if (*pDenseId_ == removeDenseId) *pDenseId_ = -1;
            world_->removeDenseScene(removeDenseId);
            if (onWorldChanged_) onWorldChanged_();
            if (*pDenseId_ < 0 && !world_->getDenseScenes().empty())
                *pDenseId_ = world_->getDenseScenes().front()->getId();
        }

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

} // namespace VkVolumeView
