#include "ViewPanel.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "App.h"
#include "imgui.h"
#include "../../CGLib/ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

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
            if (app_) {
                if (app_->hasAssetCamera()) {
                    bool useAssetCamera = app_->useAssetCamera();
                    if (ImGui::Checkbox("Use Asset Camera", &useAssetCamera))
                        app_->setUseAssetCamera(useAssetCamera);
                } else {
                    ImGui::TextDisabled("Use Asset Camera (no camera in this asset)");
                }
            }
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
            if (renderer_->hasSkyboxPipeline()) {
                if (ImGui::Checkbox("Show Skybox", &useSkybox_)) renderer_->setUseSkybox(useSkybox_);
            } else {
                ImGui::TextDisabled("Show Skybox (no skybox shaders loaded)");
            }
            if (app_) {
                ImGui::Text("%s", app_->hasEnvironmentHDR() ? "Env: real HDRI" : "Env: placeholder");
                if (ImGui::Button("Load HDRI...")) {
                    const char* filters[] = { "*.hdr" };
                    const char* path = tinyfd_openFileDialog(
                        "Load Environment HDRI", "", 1, filters, "Radiance HDR files (*.hdr)", 0);
                    if (path) app_->loadEnvironmentHDR(path);
                }
                if (app_->hasEnvironmentHDR()) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear HDRI")) app_->clearEnvironmentHDR();
                }
            }
        }
        if (app_ && ImGui::CollapsingHeader("Material")) {
            const int materialCount = renderer_->document() ? static_cast<int>(renderer_->document()->materials.size()) : 0;
            const int maxIndex = materialCount > 0 ? materialCount - 1 : 0;
            ImGui::SliderInt("Material Index", &materialIndex_, 0, maxIndex);
            if (materialCount == 0) ImGui::TextDisabled("(document has no materials -- index 0 is the implicit default)");

            const bool hasOverride = app_->hasPhmatOverride(materialIndex_);
            ImGui::Text("%s", hasOverride ? "Shader: .phmat override" : "Shader: default PBR");

            if (ImGui::Button("Load .phmat...")) {
                const char* filters[] = { "*.phmat" };
                const char* path = tinyfd_openFileDialog(
                    "Load Material Shader Graph", "", 1, filters, "Phantom material graph (*.phmat)", 0);
                if (path) {
                    std::string err;
                    if (app_->loadPhmatMaterial(materialIndex_, path, &err)) lastPhmatError_.clear();
                    else lastPhmatError_ = err;
                }
            }
            if (hasOverride) {
                ImGui::SameLine();
                if (ImGui::Button("Clear .phmat")) { app_->clearPhmatMaterial(materialIndex_); lastPhmatError_.clear(); }
            }
            if (!lastPhmatError_.empty())
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Error: %s", lastPhmatError_.c_str());
        }
    }
    ImGui::End();
}
