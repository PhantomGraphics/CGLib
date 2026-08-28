#include "MenuPanel.h"

#include "SVBoxView.h"
#include "SVSphereView.h"
#include "SVCombineView.h"
#include "SVResampleView.h"
#include "SVMeshView.h"
#include "SVParticleView.h"
#include "MCMeshView.h"
#include "DVCreateBoxView.h"
#include "DVFromSparseView.h"
#include "DVMarchingCubesView.h"
#include "DenseVolumeRenderer.h"
#include "SparseVolumeRenderer.h"
#include "VectorFieldRenderer.h"
#include "../VolumeRenderer/PBVRRenderer.h"

#include "imgui.h"

#include <memory>
#include <utility>

namespace VkVolumeView {

void MenuPanel::init(World* world, int* pActiveSceneId, int* pActiveDenseSceneId,
                            std::function<void()> onRebuild,
                            SceneListPanel* scenePanel,
                            SparseVolumeRenderer* pointRenderer,
                            DenseVolumeRenderer*  denseRenderer,
                            VectorFieldRenderer*  lineRenderer,
                            Phantom::Volume::PBVRRenderer*   pbvrRenderer,
                            std::function<void()>   onCameraReset)
{
    world_         = world;
    pId_           = pActiveSceneId;
    pDenseId_      = pActiveDenseSceneId;
    onRebuild_     = std::move(onRebuild);
    scenePanel_    = scenePanel;
    pointRenderer_ = pointRenderer;
    denseRenderer_ = denseRenderer;
    lineRenderer_  = lineRenderer;
    pbvrRenderer_  = pbvrRenderer;
    onCameraReset_ = std::move(onCameraReset);

    if (pointRenderer_) pointSize_       = pointRenderer_->getPointSize();
    if (denseRenderer_) densePointSize_  = denseRenderer_->getPointSize();
    if (denseRenderer_) denseColorMap_   = static_cast<int>(denseRenderer_->getColorMapType());
    if (pbvrRenderer_)  densityScale_    = pbvrRenderer_->getDensityScale();
    if (pbvrRenderer_)  particleSize_    = pbvrRenderer_->getParticleSize();
    if (pbvrRenderer_)  repeatCount_     = pbvrRenderer_->getRepeatCount();
    if (pbvrRenderer_)  pbvrUseGPU_     = pbvrRenderer_->isGPUMode();
    if (pbvrRenderer_)  pbvrMaxParticlesPerVoxel_ = pbvrRenderer_->getMaxParticlesPerVoxel();
    if (pbvrRenderer_)  shadowEnabled_   = pbvrRenderer_->isShadowEnabled();
    if (pbvrRenderer_)  lightAzimuth_    = pbvrRenderer_->getLightAzimuth();
    if (pbvrRenderer_)  lightElevation_  = pbvrRenderer_->getLightElevation();
    if (pbvrRenderer_)  sigma_           = pbvrRenderer_->getExtinction();
    if (pbvrRenderer_)  shadowLayers_    = pbvrRenderer_->getShadowLayers();
}

void MenuPanel::setActiveProcessView(IVolumeProcessView* view) {
    activeProcessView_.reset(view);
    activeProcessUsesDenseId_ = false;
}

// ============================================================
//  Menu bar
// ============================================================

void MenuPanel::onImGuiMenuBar() {
    if (!ImGui::BeginMenu("Volume")) return;

    ImGui::TextDisabled("Generate:");
    if (ImGui::MenuItem("Create Box SDF"))
        activeProcessView_ = std::make_unique<SVBoxView>();
    if (ImGui::MenuItem("Create Sphere SDF"))
        activeProcessView_ = std::make_unique<SVSphereView>();
    if (ImGui::MenuItem("Mesh to SDF (STL)"))
        activeProcessView_ = std::make_unique<SVMeshView>();
    if (ImGui::MenuItem("Particles to Volume (SPH)"))
        activeProcessView_ = std::make_unique<SVParticleView>();

    ImGui::Separator();

    ImGui::TextDisabled("Process:");
    if (ImGui::MenuItem("CSG Combine"))
        activeProcessView_ = std::make_unique<SVCombineView>();
    if (ImGui::MenuItem("Resample"))
        activeProcessView_ = std::make_unique<SVResampleView>();

    ImGui::Separator();

    ImGui::TextDisabled("Surface:");
    if (ImGui::MenuItem("Marching Cubes Mesh"))
        activeProcessView_ = std::make_unique<MCMeshView>();

    ImGui::Separator();

    ImGui::TextDisabled("Dense Volume:");
    if (ImGui::MenuItem("Create Dense Box")) {
        activeProcessView_ = std::make_unique<DVCreateBoxView>();
        activeProcessUsesDenseId_ = true;
    }
    if (ImGui::MenuItem("Dense From Sparse")) {
        activeProcessView_ = std::make_unique<DVFromSparseView>();
        activeProcessUsesDenseId_ = false;
    }
    if (ImGui::MenuItem("Dense Marching Cubes")) {
        activeProcessView_ = std::make_unique<DVMarchingCubesView>();
        activeProcessUsesDenseId_ = true;
    }

    ImGui::EndMenu();
}

// ============================================================
//  IVkUIPanel
// ============================================================

void MenuPanel::onImGui() {
    syncRendererStates();

    ImGui::SetNextWindowPos(ImVec2(10.f, 35.f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400.f, 620.f), ImGuiCond_Once);
    if (!ImGui::Begin("VolumeView Control")) { ImGui::End(); return; }

    if (world_) {
        int totalVoxels = 0;
        for (const auto& s : world_->getScenes())
            if (s->getShape()) totalVoxels += s->getShape()->getActiveVoxelCount();
        ImGui::Text("Total voxels: %d", totalVoxels);
        ImGui::Spacing();
    }

    if (scenePanel_) scenePanel_->onImGui();

    ImGui::Spacing();
    ImGui::Separator();

    drawRenderSettings();

    ImGui::Spacing();
    ImGui::Separator();

    drawProcessView();

    ImGui::End();
}

// ============================================================
//  Private helpers
// ============================================================

void MenuPanel::syncRendererStates() {
    if (pointRenderer_)
        pointRenderer_->setEnabled(renderMode_ == RenderMode::Points ||
                                   renderMode_ == RenderMode::Both);
    if (denseRenderer_)
        denseRenderer_->setEnabled(showDensePoints_);
    if (pbvrRenderer_)
        pbvrRenderer_->setEnabled(renderMode_ == RenderMode::PBVR ||
                                  renderMode_ == RenderMode::Both);
    if (lineRenderer_) {
        lineRenderer_->setShowVectorField(showVectorField_);
        lineRenderer_->setShowVolumeGrid(showVolumeGrid_);
        lineRenderer_->setEnabled(showVectorField_ || showVolumeGrid_);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Dense Points:");
    ImGui::Checkbox("Enable Dense Points", &showDensePoints_);
    if (ImGui::SliderFloat("Point Size##dense", &densePointSize_, 1.0f, 20.0f) && denseRenderer_)
        denseRenderer_->setPointSize(densePointSize_);

    const char* colorMapItems[] = { "Jet", "Viridis", "Grayscale" };
    if (ImGui::Combo("Color Map##dense", &denseColorMap_, colorMapItems, 3) && denseRenderer_) {
        denseRenderer_->setColorMapType(static_cast<DenseVolumeRenderer::ColorMapType>(denseColorMap_));
    }
}

void MenuPanel::drawRenderSettings() {
    ImGui::TextDisabled("Render Settings");

    // --- Render mode ---
    int mode = static_cast<int>(renderMode_);
    bool changed = false;
    changed |= ImGui::RadioButton("Voxel Points", &mode, 0); ImGui::SameLine();
    changed |= ImGui::RadioButton("PBVR",         &mode, 1); ImGui::SameLine();
    changed |= ImGui::RadioButton("Both",         &mode, 2);
    if (changed) renderMode_ = static_cast<RenderMode>(mode);

    ImGui::Spacing();

    // --- Camera reset ---
    if (ImGui::Button("Reset Camera") && onCameraReset_) onCameraReset_();

    ImGui::Spacing();

    // --- Point renderer controls ---
    const bool showPoints = (renderMode_ == RenderMode::Points || renderMode_ == RenderMode::Both);
    if (showPoints) {
        ImGui::TextDisabled("Voxel Points:");
        if (ImGui::SliderFloat("Point Size##pts", &pointSize_, 1.0f, 20.0f) && pointRenderer_)
            pointRenderer_->setPointSize(pointSize_);
    }

    // --- PBVR controls ---
    const bool showPBVR = (renderMode_ == RenderMode::PBVR || renderMode_ == RenderMode::Both);
    if (showPBVR) {
        ImGui::TextDisabled("PBVR:");
        if (ImGui::SliderFloat("Density Scale##pbvr", &densityScale_, 0.1f, 10.0f) && pbvrRenderer_)
            pbvrRenderer_->setDensityScale(densityScale_);
        if (ImGui::SliderFloat("Particle Size##pbvr", &particleSize_, 1.0f, 20.0f) && pbvrRenderer_)
            pbvrRenderer_->setParticleSize(particleSize_);
        if (ImGui::SliderInt("Repeat Count##pbvr", &repeatCount_, 1, 16) && pbvrRenderer_)
            pbvrRenderer_->setRepeatCount(repeatCount_);
        if (ImGui::Checkbox("GPU Generation##pbvr", &pbvrUseGPU_) && pbvrRenderer_)
            pbvrRenderer_->setUseGPU(pbvrUseGPU_);
        if (pbvrUseGPU_) {
            if (ImGui::SliderInt("Max Particles/Voxel##pbvr", &pbvrMaxParticlesPerVoxel_, 1, 16) && pbvrRenderer_)
                pbvrRenderer_->setMaxParticlesPerVoxel(pbvrMaxParticlesPerVoxel_);
        }
        if (pbvrRenderer_)
            ImGui::Text("Particles: %d", static_cast<int>(pbvrRenderer_->getParticleCount()));

        const char* tfPresetItems[] = { "Debug (rainbow)", "Cloud (white)" };
        if (ImGui::Combo("TF Preset##pbvr", &pbvrTFPreset_, tfPresetItems, 2) && pbvrRenderer_)
            pbvrRenderer_->setTransferFunctionPreset(pbvrTFPreset_);

        ImGui::Spacing();
        ImGui::TextDisabled("Self-Shadow (experimental):");
        if (ImGui::Checkbox("Enable Shadow##pbvr", &shadowEnabled_) && pbvrRenderer_)
            pbvrRenderer_->setShadowEnabled(shadowEnabled_);

        if (shadowEnabled_) {
            bool lightChanged = false;
            lightChanged |= ImGui::SliderFloat("Light Azimuth##pbvr", &lightAzimuth_, 0.0f, 360.0f);
            lightChanged |= ImGui::SliderFloat("Light Elevation##pbvr", &lightElevation_, 5.0f, 85.0f);
            if (lightChanged && pbvrRenderer_)
                pbvrRenderer_->setLightDir(lightAzimuth_, lightElevation_);

            if (ImGui::SliderFloat("Extinction (sigma)##pbvr", &sigma_, 0.01f, 10.0f) && pbvrRenderer_)
                pbvrRenderer_->setExtinction(sigma_);
            if (ImGui::SliderInt("Shadow Layers##pbvr", &shadowLayers_, 2, 32) && pbvrRenderer_)
                pbvrRenderer_->setShadowLayers(shadowLayers_);

            static const int kShadowSizes[] = { 256, 512, 768 };
            const char* shadowSizeItems[] = { "256", "512", "768" };
            if (ImGui::Combo("Shadow Map Size##pbvr", &shadowSizeIdx_, shadowSizeItems, 3) && pbvrRenderer_)
                pbvrRenderer_->setShadowMapSize(static_cast<uint32_t>(kShadowSizes[shadowSizeIdx_]));

            if (pbvrRenderer_) {
                const glm::vec3 dir = pbvrRenderer_->computeLightDir();
                ImGui::Text("Light Dir: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
            }
        }
    }

    ImGui::Spacing();

    // --- Overlays ---
    ImGui::Checkbox("Show Vector Field (gradient)", &showVectorField_);
    ImGui::Checkbox("Show Volume Grid (wireframe)", &showVolumeGrid_);
}

void MenuPanel::drawProcessView() {
    if (!activeProcessView_) return;

    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                       "[%s]", activeProcessView_->getName());
    ImGui::SameLine();
    if (ImGui::SmallButton("Close")) {
        activeProcessView_.reset();
        return;
    }
    ImGui::Spacing();
    if (world_) {
        const int activeId = activeProcessUsesDenseId_
            ? ((pDenseId_) ? *pDenseId_ : -1)
            : ((pId_) ? *pId_ : -1);

        activeProcessView_->onImGui(*world_, activeId,
            [this]() { if (onRebuild_) onRebuild_(); });
    }
    ImGui::Spacing();
    ImGui::Separator();
}

} // namespace VkVolumeView
