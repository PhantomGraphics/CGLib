#include "ControlPanel.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "../GltfRenderer/Gltf/GltfDocument.h"
#include "imgui.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace Phantom::Gltf;

ControlPanel::ControlPanel()
    : fileOpenView_("Open glTF")
{
    fileOpenView_.addFilter("*.gltf");
    fileOpenView_.addFilter("*.glb");
    fileOpenView_.addFilter("*.vrm");
    fileOpenView_.addFilter("*.obj");
    fileOpenView_.addFilter("*.stl");
}

void ControlPanel::onImGui() {
    ImGui::Begin("glTF Viewer");

    fileOpenView_.show();
    if (ImGui::Button("Load")) {
        const std::string path = fileOpenView_.getFileName();
        if (!path.empty() && onFileOpen_)
            onFileOpen_(std::filesystem::path(path));
    }

    ImGui::Separator();

    if (filePath_.empty()) {
        ImGui::Text("File: <none>");
    } else {
        ImGui::Text("File: %s", filePath_.filename().string().c_str());
    }
    ImGui::Separator();

    if (renderer_) {
        ImGui::SliderFloat("Camera Distance", renderer_->camDistPtr(), 0.1f, 100.f);
        ImGui::SliderFloat3("Camera Target", &renderer_->camTargetPtr()->x, -10.f, 10.f);
        ImGui::Separator();

        ImGui::Text("Light");
        bool lightChanged = false;
        lightChanged |= ImGui::DragFloat3("Light Pos",   &lightPos_.x,   0.1f);
        lightChanged |= ImGui::ColorEdit3("Light Color", &lightColor_.x);
        lightChanged |= ImGui::DragFloat("Light Intensity", &lightIntensity_, 0.1f, 0.f, 20.f);
        if (lightChanged)
            renderer_->setLight(glm::vec4(lightPos_, 0.f),
                                glm::vec4(lightColor_ * lightIntensity_, 1.f));

        if (ImGui::Checkbox("Use IBL", &useIBL_))
            renderer_->setUseIBL(useIBL_);

        ImGui::Separator();
        const GltfDocument* doc = renderer_->document();
        if (doc && !doc->meshes.empty()) {
            ImGui::Text("Meshes   : %d", (int)doc->meshes.size());
            ImGui::Text("Materials: %d", (int)doc->materials.size());
            ImGui::Text("Textures : %d", (int)doc->textures.size());
        }
    }

    if (vrmState_ && vrmState_->active) {
        ImGui::Separator();
        if (ImGui::CollapsingHeader("VRM", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* specLabel = vrmState_->specVersion == VrmSpecVersion::V1 ? "1.0"
                                   : vrmState_->specVersion == VrmSpecVersion::V0 ? "0.x"
                                   : "unknown";
            ImGui::Text("Spec: VRM %s", specLabel);
            if (!vrmState_->meta.title.empty())  ImGui::Text("Title:   %s", vrmState_->meta.title.c_str());
            if (!vrmState_->meta.author.empty()) ImGui::Text("Author:  %s", vrmState_->meta.author.c_str());
            if (!vrmState_->meta.version.empty())ImGui::Text("Version: %s", vrmState_->meta.version.c_str());
            ImGui::Text("Humanoid: %d bones mapped", (int)vrmState_->humanoid.boneNameToNode.size());

            if (!vrmState_->expressions.empty()) {
                ImGui::Separator();
                ImGui::Text("Expressions");
                for (size_t i = 0; i < vrmState_->expressions.size(); ++i) {
                    const VrmExpression& expr = vrmState_->expressions[i];
                    const std::string& shownName = expr.presetName.empty() ? expr.name : expr.presetName;
                    // "##<index>" disambiguates ImGui's widget ID in case two expressions share a
                    // display name -- does not appear in the rendered label.
                    const std::string label = shownName + "##expr" + std::to_string(i);
                    float weight = (i < vrmState_->expressionWeights.size()) ? vrmState_->expressionWeights[i] : 0.f;
                    if (ImGui::SliderFloat(label.c_str(), &weight, 0.0f, 1.0f) && onVrmExpressionChanged_) {
                        onVrmExpressionChanged_(static_cast<int>(i), weight);
                    }
                }
            }
        }
    }

    ImGui::End();
}
