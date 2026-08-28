#pragma once

#include "World.h"
#include <functional>

namespace VkVolumeView {

// Renders the scene list section inside an existing ImGui window.
// Call onImGui() from within an ImGui::Begin() / ImGui::End() block.
class SceneListPanel {
public:
    void init(World* world, int* pActiveSceneId, int* pActiveDenseSceneId,
              std::function<void()> onWorldChanged,
              std::function<void()> onSceneSelectionChanged = {});

    void onImGui();

private:
    World* world_ = nullptr;
    int*         pSparseId_ = nullptr;
    int*         pDenseId_  = nullptr;
    std::function<void()> onWorldChanged_;
    std::function<void()> onSceneSelectionChanged_;
};

} // namespace VkVolumeView
