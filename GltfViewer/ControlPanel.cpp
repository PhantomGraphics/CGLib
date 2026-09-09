#include "ControlPanel.h"
#include "imgui.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <functional>
#include <string>

using namespace Phantom::Gltf;

ControlPanel::ControlPanel() = default;

void ControlPanel::onImGui() {
    if (!visible_) return;
    if (!ImGui::Begin("Control", &visible_)) {
        ImGui::End();
        return;
    }

    if (filePath_.empty()) {
        ImGui::Text("File: <none>");
    } else {
        ImGui::Text("File: %s", filePath_.filename().string().c_str());
    }
    ImGui::Separator();

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
