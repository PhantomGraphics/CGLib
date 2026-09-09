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

    if (document_ && selectedNode_ &&
        *selectedNode_ >= 0 && *selectedNode_ < static_cast<int>(document_->nodes.size())) {
        const int nodeIndex = *selectedNode_;
        const GltfNode& node = document_->nodes[nodeIndex];
        const std::string nodeName = node.name.empty()
            ? "Node " + std::to_string(nodeIndex) : node.name;

        if (ImGui::CollapsingHeader("Selected Object", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Name: %s", nodeName.c_str());
            ImGui::Text("Node index: %d", nodeIndex);
            ImGui::Separator();
            ImGui::Text("Transform");
            if (node.hasMatrix) {
                for (int row = 0; row < 4; ++row) {
                    ImGui::Text("  %.4f  %.4f  %.4f  %.4f",
                        node.matrix[0][row], node.matrix[1][row],
                        node.matrix[2][row], node.matrix[3][row]);
                }
            } else {
                ImGui::Text("  Translation: %.4f, %.4f, %.4f",
                    node.translation.x, node.translation.y, node.translation.z);
                ImGui::Text("  Rotation:    %.4f, %.4f, %.4f, %.4f",
                    node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w);
                ImGui::Text("  Scale:       %.4f, %.4f, %.4f",
                    node.scale.x, node.scale.y, node.scale.z);
            }
            ImGui::Text("Children: %d", static_cast<int>(node.children.size()));

            if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(document_->meshes.size())) {
                const GltfMesh& mesh = document_->meshes[node.meshIndex];
                const std::string meshName = mesh.name.empty()
                    ? "Mesh " + std::to_string(node.meshIndex) : mesh.name;
                ImGui::Separator();
                ImGui::Text("Mesh: %s", meshName.c_str());
                ImGui::Text("Primitives: %d", static_cast<int>(mesh.primitives.size()));
            }
            if (node.skin >= 0 && node.skin < static_cast<int>(document_->skins.size())) {
                const GltfSkin& skin = document_->skins[node.skin];
                ImGui::Text("Skin: %s (%d joints)",
                    skin.name.empty() ? "Skin " + std::to_string(node.skin) : skin.name,
                    static_cast<int>(skin.joints.size()));
            }
            if (node.cameraIndex >= 0 && node.cameraIndex < static_cast<int>(document_->cameras.size())) {
                const GltfCamera& camera = document_->cameras[node.cameraIndex];
                ImGui::Separator();
                ImGui::Text("Camera: %s (%s)",
                    camera.name.empty() ? "Camera " + std::to_string(node.cameraIndex) : camera.name,
                    camera.type.c_str());
            }
            if (node.lightIndex >= 0 && node.lightIndex < static_cast<int>(document_->lights.size())) {
                const GltfLight& light = document_->lights[node.lightIndex];
                ImGui::Text("Light: %s (%s)",
                    light.name.empty() ? "Light " + std::to_string(node.lightIndex) : light.name,
                    light.type.c_str());
            }
        }
    } else {
        ImGui::TextDisabled("Select an object in the Scene Graph.");
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
