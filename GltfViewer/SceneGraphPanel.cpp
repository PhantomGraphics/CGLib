#include "SceneGraphPanel.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <vector>

using namespace Phantom::Gltf;

void SceneGraphPanel::onImGui() {
    if (!visible_ || !document_) return;
    if (!ImGui::Begin("Scene Graph", &visible_)) {
        ImGui::End();
        return;
    }

    const GltfDocument& doc = *document_;
    if (ImGui::TreeNodeEx("World", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen)) {
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
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc.nodes.size())) return;
            if (visiting[nodeIndex]) {
                ImGui::BulletText("Node %d (cycle)", nodeIndex);
                return;
            }

            const GltfNode& node = doc.nodes[nodeIndex];
            const bool hasMesh = node.meshIndex >= 0 &&
                node.meshIndex < static_cast<int>(doc.meshes.size());
            const bool hasCamera = node.cameraIndex >= 0 &&
                node.cameraIndex < static_cast<int>(doc.cameras.size());
            const bool hasLight = node.lightIndex >= 0 &&
                node.lightIndex < static_cast<int>(doc.lights.size());
            const bool hasChildren = !node.children.empty() || hasCamera || hasLight;
            const ImGuiTreeNodeFlags flags = (hasMesh || hasChildren)
                ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf;
            const bool open = ImGui::TreeNodeEx(nodeLabel(node, nodeIndex).c_str(), flags);
            if (!open) return;

            visiting[nodeIndex] = true;
            if (hasMesh) {
                const GltfMesh& mesh = doc.meshes[node.meshIndex];
                if (ImGui::TreeNodeEx(meshLabel(mesh, node.meshIndex).c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
                        const GltfPrimitive& primitive = mesh.primitives[primitiveIndex];
                        const int materialIndex = primitive.materialIndex;
                        std::string label = "Primitive " + std::to_string(primitiveIndex);
                        if (materialIndex >= 0 && materialIndex < static_cast<int>(doc.materials.size())) {
                            const auto& material = doc.materials[materialIndex];
                            label += "  [" + (material.name.empty()
                                ? "Material " + std::to_string(materialIndex) : material.name) + "]";
                        }
                        ImGui::BulletText("%s", label.c_str());
                    }
                    ImGui::TreePop();
                }
            }
            if (hasCamera) {
                const auto& camera = doc.cameras[node.cameraIndex];
                const std::string label = "Camera: " +
                    (camera.name.empty() ? "Camera " + std::to_string(node.cameraIndex) : camera.name) +
                    " (" + camera.type + ")##camera" + std::to_string(node.cameraIndex);
                ImGui::BulletText("%s", label.c_str());
            }
            if (hasLight) {
                const auto& light = doc.lights[node.lightIndex];
                const std::string label = "Light: " +
                    (light.name.empty() ? "Light " + std::to_string(node.lightIndex) : light.name) +
                    " (" + light.type + ")##light" + std::to_string(node.lightIndex);
                ImGui::BulletText("%s", label.c_str());
            }
            for (int child : node.children) drawNode(child, visiting);
            visiting[nodeIndex] = false;
            ImGui::TreePop();
        };

        std::vector<bool> visiting(doc.nodes.size(), false);
        if (doc.scenes.empty()) {
            std::vector<bool> hasParent(doc.nodes.size(), false);
            for (const auto& node : doc.nodes)
                for (int child : node.children)
                    if (child >= 0 && child < static_cast<int>(hasParent.size())) hasParent[child] = true;
            for (int i = 0; i < static_cast<int>(doc.nodes.size()); ++i)
                if (!hasParent[i]) drawNode(i, visiting);
        } else {
            for (size_t sceneIndex = 0; sceneIndex < doc.scenes.size(); ++sceneIndex) {
                const auto& scene = doc.scenes[sceneIndex];
                const std::string sceneName = scene.name.empty()
                    ? "Scene " + std::to_string(sceneIndex) : scene.name;
                if (ImGui::TreeNodeEx((sceneName + "##scene" + std::to_string(sceneIndex)).c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (int root : scene.nodes) drawNode(root, visiting);
                    ImGui::TreePop();
                }
            }
        }
        if (doc.nodes.empty()) ImGui::TextDisabled("(no nodes)");
    }

    const auto drawResources = [](const char* title, size_t count, auto labelFor) {
        if (!ImGui::TreeNode(title)) return;
        for (size_t i = 0; i < count; ++i) {
            const std::string label = labelFor(i) + "##resource" + std::to_string(i);
            ImGui::BulletText("%s", label.c_str());
        }
        ImGui::TreePop();
    };
    drawResources("Meshes", doc.meshes.size(), [&](size_t i) {
        return doc.meshes[i].name.empty() ? "Mesh " + std::to_string(i) : doc.meshes[i].name;
    });
    drawResources("Materials", doc.materials.size(), [&](size_t i) {
        return doc.materials[i].name.empty() ? "Material " + std::to_string(i) : doc.materials[i].name;
    });
    drawResources("Textures", doc.textures.size(), [&](size_t i) {
        return "Texture " + std::to_string(i);
    });
    drawResources("Skins", doc.skins.size(), [&](size_t i) {
        return doc.skins[i].name.empty() ? "Skin " + std::to_string(i) : doc.skins[i].name;
    });
    drawResources("Animations", doc.animations.size(), [&](size_t i) {
        return doc.animations[i].name.empty() ? "Animation " + std::to_string(i) : doc.animations[i].name;
    });
    drawResources("Cameras", doc.cameras.size(), [&](size_t i) {
        return (doc.cameras[i].name.empty() ? "Camera " + std::to_string(i) : doc.cameras[i].name)
             + " (" + doc.cameras[i].type + ")";
    });
    drawResources("Lights", doc.lights.size(), [&](size_t i) {
        return (doc.lights[i].name.empty() ? "Light " + std::to_string(i) : doc.lights[i].name)
             + " (" + doc.lights[i].type + ")";
    });
    ImGui::TreePop();
    }

    ImGui::End();
}
