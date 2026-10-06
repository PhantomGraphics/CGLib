#include "ViewPanel.h"
#include <cstdio>
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "App.h"
#include "imgui.h"
#include "../../CGLib/ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

using namespace Phantom::Gltf;

namespace {
std::string fnum(float v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    return buf;
}
}

void ViewPanel::setLight(const glm::vec3& pos, const glm::vec3& color, float intensity) {
    lightPos_ = pos;
    lightColor_ = color;
    lightIntensity_ = intensity;
    if (renderer_)
        renderer_->setLight(glm::vec4(lightPos_, 0.f), glm::vec4(lightColor_ * lightIntensity_, 1.f));
}

void ViewPanel::onImGui() {
    if (!visible_) return;
    if (!ImGui::Begin("View", &visible_)) {
        ImGui::End();
        return;
    }

    if (renderer_) {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            float dist = *renderer_->camDistPtr();
            if (ImGui::SliderFloat("Distance", &dist, 0.1f, 100.f)) emit("SetCamDist:" + fnum(dist));
            glm::vec3 target = *renderer_->camTargetPtr();
            if (ImGui::SliderFloat3("Target", &target.x, -10.f, 10.f))
                emit("SetCamTarget:" + fnum(target.x) + ":" + fnum(target.y) + ":" + fnum(target.z));
            if (app_) {
                if (app_->hasAssetCamera()) {
                    bool useAssetCamera = app_->useAssetCamera();
                    if (ImGui::Checkbox("Use Asset Camera", &useAssetCamera))
                        emit(std::string("SetUseAssetCamera:") + (useAssetCamera ? "1" : "0"));
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
                emit("SetLight:" + fnum(lightPos_.x) + ":" + fnum(lightPos_.y) + ":" + fnum(lightPos_.z) + ":" +
                     fnum(lightColor_.x) + ":" + fnum(lightColor_.y) + ":" + fnum(lightColor_.z) + ":" +
                     fnum(lightIntensity_));
        }
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            useIBL_ = renderer_->getUseIBL() != 0;      // renderer is the single source of truth
            useSkybox_ = renderer_->getUseSkybox();
            if (ImGui::Checkbox("Use IBL", &useIBL_)) emit(std::string("SetUseIBL:") + (useIBL_ ? "1" : "0"));
            if (renderer_->hasSkyboxPipeline()) {
                if (ImGui::Checkbox("Show Skybox", &useSkybox_)) emit(std::string("SetUseSkybox:") + (useSkybox_ ? "1" : "0"));
            } else {
                ImGui::TextDisabled("Show Skybox (no skybox shaders loaded)");
            }
            if (app_) {
                ImGui::Text("%s", app_->hasEnvironmentHDR() ? "Env: real HDRI" : "Env: placeholder");
                if (ImGui::Button("Load HDRI...")) {
                    const char* filters[] = { "*.hdr" };
                    const char* path = tinyfd_openFileDialog(
                        "Load Environment HDRI", "", 1, filters, "Radiance HDR files (*.hdr)", 0);
                    if (path) emit(std::string("LoadEnvironmentHDR:") + path);
                }
                if (app_->hasEnvironmentHDR()) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear HDRI")) emit("ClearEnvironmentHDR");
                }
            }
        }
        if (app_ && ImGui::CollapsingHeader("Material")) {
            const int materialCount = renderer_->document() ? static_cast<int>(renderer_->document()->materials.size()) : 0;
            const int maxIndex = materialCount > 0 ? materialCount - 1 : 0;
            ImGui::SliderInt("Material Index", &materialIndex_, 0, maxIndex);
            if (ImGui::Button("Show Shader Graph")) emit("ShowShaderGraph:" + std::to_string(materialIndex_));
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
                if (ImGui::Button("Clear .phmat")) { emit("ClearPhmat:" + std::to_string(materialIndex_)); lastPhmatError_.clear(); }
            }
            if (!lastPhmatError_.empty())
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Error: %s", lastPhmatError_.c_str());

            // Phase 4C item 5: opt-in hot reload (App::checkPhmatHotReload()) + pipeline-variant
            // pooling stat, for visibility into "shader variant"/"pipeline cache" reuse.
            bool hotReload = app_->phmatHotReloadEnabled();
            if (ImGui::Checkbox("Watch .phmat/.phshader for changes", &hotReload))
                emit(std::string("SetPhmatHotReload:") + (hotReload ? "1" : "0"));
            ImGui::TextDisabled("Pipeline variants in use: %d", app_->phmatPipelineVariantCount());
        }
    }
    ImGui::End();
}
