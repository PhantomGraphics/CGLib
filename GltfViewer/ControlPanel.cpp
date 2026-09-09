#include "ControlPanel.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "../GltfRenderer/Gltf/GltfDocument.h"
#include "imgui.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <vector>

using namespace Phantom::Gltf;

ControlPanel::ControlPanel() = default;

void ControlPanel::onImGui() {
    ImGui::Begin("glTF Viewer");

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

        if (doc) {
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Scene Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto nodeLabel = [](const GltfNode& node, int index) {
                    return (node.name.empty() ? "Node " + std::to_string(index) : node.name)
                         + "##node" + std::to_string(index);
                };
                const auto meshLabel = [](const GltfMesh& mesh, int index) {
                    return (mesh.name.empty() ? "Mesh " + std::to_string(index) : mesh.name)
                         + "##mesh" + std::to_string(index);
                };

                std::function<void(int, std::vector<bool>&)> drawNode;
                drawNode = [&](int nodeIndex, std::vector<bool>& visiting) {
                    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return;
                    if (visiting[nodeIndex]) {
                        ImGui::BulletText("Node %d (cycle)", nodeIndex);
                        return;
                    }

                    const GltfNode& node = doc->nodes[nodeIndex];
                    const bool hasMesh = node.meshIndex >= 0 &&
                        node.meshIndex < static_cast<int>(doc->meshes.size());
                    const bool hasChildren = !node.children.empty();
                    const ImGuiTreeNodeFlags flags = (hasMesh || hasChildren)
                        ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf;
                    const bool open = ImGui::TreeNodeEx(nodeLabel(node, nodeIndex).c_str(), flags);
                    if (!open) return;

                    visiting[nodeIndex] = true;
                    if (hasMesh) {
                        const GltfMesh& mesh = doc->meshes[node.meshIndex];
                        if (ImGui::TreeNodeEx(meshLabel(mesh, node.meshIndex).c_str(),
                                              ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
                                const GltfPrimitive& primitive = mesh.primitives[primitiveIndex];
                                const int materialIndex = primitive.materialIndex;
                                std::string label = "Primitive " + std::to_string(primitiveIndex);
                                if (materialIndex >= 0 && materialIndex < static_cast<int>(doc->materials.size())) {
                                    const auto& material = doc->materials[materialIndex];
                                    label += "  [" + (material.name.empty()
                                        ? "Material " + std::to_string(materialIndex) : material.name) + "]";
                                }
                                ImGui::BulletText("%s", label.c_str());
                            }
                            ImGui::TreePop();
                        }
                    }
                    for (int child : node.children) drawNode(child, visiting);
                    visiting[nodeIndex] = false;
                    ImGui::TreePop();
                };

                std::vector<bool> visiting(doc->nodes.size(), false);
                if (doc->scenes.empty()) {
                    std::vector<bool> hasParent(doc->nodes.size(), false);
                    for (const auto& node : doc->nodes)
                        for (int child : node.children)
                            if (child >= 0 && child < static_cast<int>(hasParent.size())) hasParent[child] = true;
                    for (int i = 0; i < static_cast<int>(doc->nodes.size()); ++i)
                        if (!hasParent[i]) drawNode(i, visiting);
                } else {
                    for (size_t sceneIndex = 0; sceneIndex < doc->scenes.size(); ++sceneIndex) {
                        const auto& scene = doc->scenes[sceneIndex];
                        const std::string sceneName = scene.name.empty()
                            ? "Scene " + std::to_string(sceneIndex) : scene.name;
                        if (ImGui::TreeNodeEx((sceneName + "##scene" + std::to_string(sceneIndex)).c_str(),
                                              ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (int root : scene.nodes) drawNode(root, visiting);
                            ImGui::TreePop();
                        }
                    }
                }

                if (doc->nodes.empty()) ImGui::TextDisabled("(no nodes)");
            }

            const auto drawResources = [](const char* title, size_t count, auto labelFor) {
                if (!ImGui::TreeNode(title)) return;
                for (size_t i = 0; i < count; ++i) {
                    const std::string label = labelFor(i) + "##resource" + std::to_string(i);
                    ImGui::BulletText("%s", label.c_str());
                }
                ImGui::TreePop();
            };
            drawResources("Meshes", doc->meshes.size(), [&](size_t i) {
                return doc->meshes[i].name.empty() ? "Mesh " + std::to_string(i) : doc->meshes[i].name;
            });
            drawResources("Materials", doc->materials.size(), [&](size_t i) {
                return doc->materials[i].name.empty() ? "Material " + std::to_string(i) : doc->materials[i].name;
            });
            drawResources("Textures", doc->textures.size(), [&](size_t i) {
                return "Texture " + std::to_string(i);
            });
            drawResources("Skins", doc->skins.size(), [&](size_t i) {
                return doc->skins[i].name.empty() ? "Skin " + std::to_string(i) : doc->skins[i].name;
            });
            drawResources("Animations", doc->animations.size(), [&](size_t i) {
                return doc->animations[i].name.empty() ? "Animation " + std::to_string(i) : doc->animations[i].name;
            });
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
