#include "WorldPanel.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <vector>

namespace Phantom::Gltf {

void WorldPanel::onImGui(const GltfDocument& doc, int& selectedNode) const {
    if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto nodeLabel = [](const GltfNode& node, int index, const std::string& idPath) {
            return (node.name.empty() ? "Node " + std::to_string(index) : node.name)
                 + "##node" + idPath;
        };
        const auto meshLabel = [](const GltfMesh& mesh, int index) {
            return (mesh.name.empty() ? "Mesh " + std::to_string(index) : mesh.name)
                 + "##mesh" + std::to_string(index);
        };

        std::function<void(int, std::vector<bool>&, const std::string&)> drawNode;
        drawNode = [&](int nodeIndex, std::vector<bool>& visiting, const std::string& idPath) {
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
                ? ImGuiTreeNodeFlags_SpanAvailWidth : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            const ImGuiTreeNodeFlags selectedFlags = selectedNode == nodeIndex
                ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
            const bool open = ImGui::TreeNodeEx(nodeLabel(node, nodeIndex, idPath).c_str(), flags | selectedFlags);
            if (ImGui::IsItemClicked()) selectedNode = nodeIndex;
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
            for (size_t childIndex = 0; childIndex < node.children.size(); ++childIndex) {
                drawNode(node.children[childIndex], visiting,
                         idPath + "/" + std::to_string(childIndex));
            }
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
                if (!hasParent[i]) drawNode(i, visiting, "orphan/" + std::to_string(i));
        } else {
            for (size_t sceneIndex = 0; sceneIndex < doc.scenes.size(); ++sceneIndex) {
                const auto& scene = doc.scenes[sceneIndex];
                const std::string sceneName = scene.name.empty()
                    ? "Scene " + std::to_string(sceneIndex) : scene.name;
                if (ImGui::TreeNodeEx((sceneName + "##scene" + std::to_string(sceneIndex)).c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (size_t rootIndex = 0; rootIndex < scene.nodes.size(); ++rootIndex) {
                        drawNode(scene.nodes[rootIndex], visiting,
                                 "scene/" + std::to_string(sceneIndex) + "/" + std::to_string(rootIndex));
                    }
                    ImGui::TreePop();
                }
            }
        }
        if (doc.nodes.empty()) ImGui::TextDisabled("(no nodes)");
    }

}

} // namespace Phantom::Gltf
