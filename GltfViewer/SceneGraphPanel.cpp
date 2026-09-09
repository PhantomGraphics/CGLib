#include "SceneGraphPanel.h"

#include "imgui.h"

#include <string>

using namespace Phantom::Gltf;

void SceneGraphPanel::onImGui() {
    if (!visible_ || !document_) return;
    if (!ImGui::Begin("Scene Graph", &visible_)) {
        ImGui::End();
        return;
    }

    const GltfDocument& doc = *document_;
    if (selectedNode_) world_.onImGui(doc, *selectedNode_);

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
    ImGui::End();
}
