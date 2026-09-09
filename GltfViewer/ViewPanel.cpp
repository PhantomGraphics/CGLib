#include "ViewPanel.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "imgui.h"

using namespace Phantom::Gltf;

void ViewPanel::onImGui() {
    if (!visible_) return;
    if (!ImGui::Begin("View", &visible_)) {
        ImGui::End();
        return;
    }

    if (renderer_) {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Distance", renderer_->camDistPtr(), 0.1f, 100.f);
            ImGui::SliderFloat3("Target", &renderer_->camTargetPtr()->x, -10.f, 10.f);
        }
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool changed = false;
            changed |= ImGui::DragFloat3("Position", &lightPos_.x, 0.1f);
            changed |= ImGui::ColorEdit3("Color", &lightColor_.x);
            changed |= ImGui::DragFloat("Intensity", &lightIntensity_, 0.1f, 0.f, 20.f);
            if (changed)
                renderer_->setLight(glm::vec4(lightPos_, 0.f),
                                    glm::vec4(lightColor_ * lightIntensity_, 1.f));
        }
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Use IBL", &useIBL_)) renderer_->setUseIBL(useIBL_);
        }
    }
    ImGui::End();
}
