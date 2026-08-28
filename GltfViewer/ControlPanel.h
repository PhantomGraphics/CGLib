#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "FileOpenView.h"
#include "VrmViewState.h"
#include <filesystem>
#include <functional>

namespace Phantom::Gltf {

    class GltfSceneRenderer;

    class ControlPanel : public ::VKG::IVkUIPanel {
    public:
        ControlPanel();
        void setRenderer(GltfSceneRenderer* r) { renderer_ = r; }
        void setFilePath(const std::filesystem::path& p) { filePath_ = p; }
        void setOnFileOpen(std::function<void(const std::filesystem::path&)> cb) { onFileOpen_ = std::move(cb); }
        // Non-owning; caller (GltfViewerApp) must keep the pointee alive and update it on every
        // load (including back to a default-constructed VrmViewState for a non-VRM file) --
        // panel just reads through it each frame. Pass nullptr to hide the VRM section entirely.
        void setVrmState(const VrmViewState* state) { vrmState_ = state; }
        void setOnVrmExpressionChanged(std::function<void(int, float)> cb) { onVrmExpressionChanged_ = std::move(cb); }
        void onImGui() override;
    private:
        GltfSceneRenderer* renderer_ = nullptr;
        std::filesystem::path filePath_;
        std::function<void(const std::filesystem::path&)> onFileOpen_;
        Phantom::UI::FileOpenView fileOpenView_;

        const VrmViewState* vrmState_ = nullptr;
        std::function<void(int, float)> onVrmExpressionChanged_;

        // Light state
        glm::vec3 lightPos_ = { 1.f, 2.f, 1.f };
        glm::vec3 lightColor_ = { 1.f, 1.f, 1.f };
        float     lightIntensity_ = 3.f;
        bool      useIBL_ = true;
    };

}