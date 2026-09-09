#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "VrmViewState.h"
#include <filesystem>
#include <functional>

namespace Phantom::Gltf {

    class ControlPanel : public ::VKG::IVkUIPanel {
    public:
        ControlPanel();
        void setFilePath(const std::filesystem::path& p) { filePath_ = p; }
        void setVisible(bool visible) { visible_ = visible; }
        bool isVisible() const { return visible_; }
        // Non-owning; caller (GltfViewerApp) must keep the pointee alive and update it on every
        // load (including back to a default-constructed VrmViewState for a non-VRM file) --
        // panel just reads through it each frame. Pass nullptr to hide the VRM section entirely.
        void setVrmState(const VrmViewState* state) { vrmState_ = state; }
        void setOnVrmExpressionChanged(std::function<void(int, float)> cb) { onVrmExpressionChanged_ = std::move(cb); }
        void onImGui() override;
    private:
        std::filesystem::path filePath_;
        bool visible_ = true;

        const VrmViewState* vrmState_ = nullptr;
        std::function<void(int, float)> onVrmExpressionChanged_;

    };

}
