#include <cstdio>
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
#include "../../VkAppBase/ScenarioRunner/ViewShell.h"

#include <memory>
#include <utility>

namespace VolumeView {

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
    if (pbvrRenderer_)  pbvrMultipleScattering_ = pbvrRenderer_->isMultipleScatteringEnabled();
    if (pbvrRenderer_)  pbvrScatteringOrders_ = pbvrRenderer_->getScatteringOrders();
    if (pbvrRenderer_)  pbvrProbeCount_ = pbvrRenderer_->getProbeCount();
    if (pbvrRenderer_)  pbvrProbeRadius_ = pbvrRenderer_->getProbeRadius();
    if (pbvrRenderer_)  pbvrPhaseG_ = pbvrRenderer_->getPhaseG();
    if (pbvrRenderer_)  pbvrScatteringAlbedo_ = pbvrRenderer_->getScatteringAlbedo();
    if (pbvrRenderer_)  pbvrScatteringExposure_ = pbvrRenderer_->getScatteringExposure();
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

    if (!shell_ || !shell_->beginPanel("VolumeView Control")) return;
    ImGui::BeginDisabled(locked_);

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

    ImGui::EndDisabled();
    shell_->endPanel();
}

// ============================================================
//  Private helpers
// ============================================================

void MenuPanel::setRenderMode(const int mode) {
    renderMode_ = static_cast<RenderMode>(mode);
    if (pointRenderer_)
        pointRenderer_->setEnabled(renderMode_ == RenderMode::Points || renderMode_ == RenderMode::Both);
    if (pbvrRenderer_)
        pbvrRenderer_->setEnabled(renderMode_ == RenderMode::PBVR || renderMode_ == RenderMode::Both);
}

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

}

namespace {
std::string fnum(const float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    return buf;
}
const int kShadowSizes[] = { 256, 512, 768 };
} // namespace

void MenuPanel::setVoxelPointSize(const float s) {
    pointSize_ = s;
    if (pointRenderer_) pointRenderer_->setPointSize(s);
}

void MenuPanel::setDensePointSize(const float s) {
    densePointSize_ = s;
    if (denseRenderer_) denseRenderer_->setPointSize(s);
}

void MenuPanel::setDenseColorMap(const int m) {
    denseColorMap_ = m;
    if (denseRenderer_) denseRenderer_->setColorMapType(static_cast<DenseVolumeRenderer::ColorMapType>(m));
}

// Single source of truth is the renderer: the widgets below show what the renderer holds, so
// a value changed by a typed/scenario command appears here without a second copy to drift.
void MenuPanel::pullRendererState() {
    if (pointRenderer_) pointSize_ = pointRenderer_->getPointSize();
    if (denseRenderer_) {
        densePointSize_ = denseRenderer_->getPointSize();
        denseColorMap_  = static_cast<int>(denseRenderer_->getColorMapType());
    }
    if (!pbvrRenderer_) return;
    const auto& r = *pbvrRenderer_;
    densityScale_ = r.getDensityScale();
    particleSize_ = r.getParticleSize();
    repeatCount_  = r.getRepeatCount();
    pbvrUseGPU_   = r.isGPUMode();
    pbvrMaxParticlesPerVoxel_ = r.getMaxParticlesPerVoxel();
    pbvrMultipleScattering_   = r.isMultipleScatteringEnabled();
    pbvrScatteringOrders_     = r.getScatteringOrders();
    pbvrProbeCount_           = r.getProbeCount();
    pbvrProbeRadius_          = r.getProbeRadius();
    pbvrPhaseG_               = r.getPhaseG();
    pbvrScatteringAlbedo_     = r.getScatteringAlbedo();
    pbvrScatteringExposure_   = r.getScatteringExposure();
    pbvrTFPreset_             = r.getTransferFunctionPreset();
    shadowEnabled_            = r.isShadowEnabled();
    lightAzimuth_             = r.getLightAzimuth();
    lightElevation_           = r.getLightElevation();
    sigma_                    = r.getExtinction();
    shadowLayers_             = r.getShadowLayers();
    for (int i = 0; i < 3; ++i)
        if (static_cast<uint32_t>(kShadowSizes[i]) == r.getShadowMapSize()) shadowSizeIdx_ = i;
}

void MenuPanel::drawRenderSettings() {
    pullRendererState();
    ImGui::TextDisabled("Render Settings");

    // --- Render mode ---
    int mode = static_cast<int>(renderMode_);
    bool changed = false;
    changed |= ImGui::RadioButton("Voxel Points", &mode, 0); ImGui::SameLine();
    changed |= ImGui::RadioButton("PBVR",         &mode, 1); ImGui::SameLine();
    changed |= ImGui::RadioButton("Both",         &mode, 2);
    if (changed) emit("SetPBVRRenderMode:" + std::to_string(mode));

    ImGui::Spacing();

    // --- Camera reset ---
    if (ImGui::Button("Reset Camera")) emit("ResetCamera");

    ImGui::Spacing();

    // --- Point renderer controls ---
    const bool showPoints = (renderMode_ == RenderMode::Points || renderMode_ == RenderMode::Both);
    if (showPoints) {
        ImGui::TextDisabled("Voxel Points:");
        if (ImGui::SliderFloat("Point Size##pts", &pointSize_, 1.0f, 20.0f))
            emit("SetVoxelPointSize:" + fnum(pointSize_));
    }

    // --- PBVR controls ---
    const bool showPBVR = (renderMode_ == RenderMode::PBVR || renderMode_ == RenderMode::Both);
    if (showPBVR) {
        ImGui::TextDisabled("PBVR:");
        if (ImGui::SliderFloat("Density Scale##pbvr", &densityScale_, 0.1f, 10.0f))
            emit("SetPBVRDensityScale:" + fnum(densityScale_));
        if (ImGui::SliderFloat("Particle Size##pbvr", &particleSize_, 1.0f, 20.0f))
            emit("SetPBVRParticleSize:" + fnum(particleSize_));
        if (ImGui::SliderInt("Repeat Count##pbvr", &repeatCount_, 1, 16))
            emit("SetPBVRRepeatCount:" + std::to_string(repeatCount_));
        if (ImGui::Checkbox("GPU Generation##pbvr", &pbvrUseGPU_))
            emit(std::string("SetPBVRUseGPU:") + (pbvrUseGPU_ ? "1" : "0"));
        if (pbvrUseGPU_) {
            if (ImGui::SliderInt("Max Particles/Voxel##pbvr", &pbvrMaxParticlesPerVoxel_, 1, 16))
                emit("SetPBVRMaxParticlesPerVoxel:" + std::to_string(pbvrMaxParticlesPerVoxel_));
        }
        if (pbvrRenderer_)
            ImGui::Text("Particles: %d", static_cast<int>(pbvrRenderer_->getParticleCount()));

        ImGui::Spacing();
        ImGui::TextDisabled("Particle Probe Multiple Scattering:");
        if (ImGui::Checkbox("Enable Multiple Scattering##pbvr", &pbvrMultipleScattering_))
            emit(std::string("SetPBVRMultipleScattering:") + (pbvrMultipleScattering_ ? "1" : "0"));
        if (pbvrMultipleScattering_ && pbvrRenderer_) {
            if (ImGui::SliderInt("Scattering Orders##pbvr", &pbvrScatteringOrders_, 0, 8))
                emit("SetPBVRScatteringOrders:" + std::to_string(pbvrScatteringOrders_));
            if (ImGui::SliderInt("Probe Count##pbvr", &pbvrProbeCount_, 1, 2048))
                emit("SetPBVRProbeCount:" + std::to_string(pbvrProbeCount_));
            if (ImGui::SliderFloat("Probe Radius##pbvr", &pbvrProbeRadius_, 0.1f, 10.0f))
                emit("SetPBVRProbeRadius:" + fnum(pbvrProbeRadius_));
            if (ImGui::SliderFloat("Phase g##pbvr", &pbvrPhaseG_, -0.99f, 0.99f))
                emit("SetPBVRPhaseG:" + fnum(pbvrPhaseG_));
            if (ImGui::SliderFloat("Scattering Albedo##pbvr", &pbvrScatteringAlbedo_, 0.0f, 1.0f))
                emit("SetPBVRScatteringAlbedo:" + fnum(pbvrScatteringAlbedo_));
            if (ImGui::SliderFloat("Exposure##pbvr", &pbvrScatteringExposure_, 0.1f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic))
                emit("SetPBVRScatteringExposure:" + fnum(pbvrScatteringExposure_));
        }

        const char* tfPresetItems[] = { "Debug (rainbow)", "Cloud (white)" };
        if (ImGui::Combo("TF Preset##pbvr", &pbvrTFPreset_, tfPresetItems, 2))
            emit("SetPBVRTFPreset:" + std::to_string(pbvrTFPreset_));

        ImGui::Spacing();
        ImGui::TextDisabled("Self-Shadow (experimental):");
        if (ImGui::Checkbox("Enable Shadow##pbvr", &shadowEnabled_))
            emit(std::string("SetPBVRShadowEnabled:") + (shadowEnabled_ ? "1" : "0"));

        if (shadowEnabled_) {
            bool lightChanged = false;
            lightChanged |= ImGui::SliderFloat("Light Azimuth##pbvr", &lightAzimuth_, 0.0f, 360.0f);
            lightChanged |= ImGui::SliderFloat("Light Elevation##pbvr", &lightElevation_, 5.0f, 85.0f);
            if (lightChanged)
                emit("SetPBVRLightDir:" + fnum(lightAzimuth_) + ":" + fnum(lightElevation_));

            if (ImGui::SliderFloat("Extinction (sigma)##pbvr", &sigma_, 0.01f, 10.0f))
                emit("SetPBVRExtinction:" + fnum(sigma_));
            if (ImGui::SliderInt("Shadow Layers##pbvr", &shadowLayers_, 2, 32))
                emit("SetPBVRShadowLayers:" + std::to_string(shadowLayers_));

            const char* shadowSizeItems[] = { "256", "512", "768" };
            if (ImGui::Combo("Shadow Map Size##pbvr", &shadowSizeIdx_, shadowSizeItems, 3))
                emit("SetPBVRShadowMapSize:" + std::to_string(kShadowSizes[shadowSizeIdx_]));

            if (pbvrRenderer_) {
                const glm::vec3 dir = pbvrRenderer_->computeLightDir();
                ImGui::Text("Light Dir: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Dense Points:");
    if (ImGui::Checkbox("Enable Dense Points", &showDensePoints_))
        emit(std::string("SetDensePoints:") + (showDensePoints_ ? "1" : "0"));
    if (ImGui::SliderFloat("Point Size##dense", &densePointSize_, 1.0f, 20.0f))
        emit("SetDensePointSize:" + fnum(densePointSize_));
    const char* colorMapItems[] = { "Jet", "Viridis", "Grayscale" };
    if (ImGui::Combo("Color Map##dense", &denseColorMap_, colorMapItems, 3))
        emit("SetDenseColorMap:" + std::to_string(denseColorMap_));

    ImGui::Spacing();

    // --- Overlays ---
    bool grid = showVolumeGrid_, field = showVectorField_;
    if (ImGui::Checkbox("Show Vector Field (gradient)", &field))
        emit(std::string("SetShowVectorField:") + (field ? "1" : "0"));
    if (ImGui::Checkbox("Show Volume Grid (wireframe)", &grid))
        emit(std::string("SetShowVolumeGrid:") + (grid ? "1" : "0"));
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

} // namespace VolumeView
