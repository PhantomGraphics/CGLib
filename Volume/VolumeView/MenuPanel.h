#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "IVolumeProcessView.h"
#include "SceneListPanel.h"

#include <functional>
#include <memory>

namespace Phantom::Volume { class PBVRRenderer; }

namespace VkVolumeView {

class SparseVolumeRenderer;
class DenseVolumeRenderer;
class VectorFieldRenderer;

enum class RenderMode { Points, PBVR, Both };

class MenuPanel : public ::VKG::IVkUIPanel {
public:
	// TODO 引数を整理する
    void init(World* world, int* pActiveSceneId, int* pActiveDenseSceneId,
              std::function<void()> onRebuild,
              SceneListPanel* scenePanel,
              SparseVolumeRenderer* pointRenderer,
              DenseVolumeRenderer*  denseRenderer,
              VectorFieldRenderer*  lineRenderer,
              Phantom::Volume::PBVRRenderer*   pbvrRenderer,
              std::function<void()>   onCameraReset);

    void onImGuiMenuBar();
    void onImGui() override;

    void setActiveProcessView(IVolumeProcessView* view);

    void setShowVolumeGrid(bool show)  { showVolumeGrid_  = show; }
    void setShowVectorField(bool show) { showVectorField_ = show; }
    bool getShowVolumeGrid()  const { return showVolumeGrid_; }
    bool getShowVectorField() const { return showVectorField_; }

    // Render mode is otherwise only reachable via the ImGui radio buttons; scenario tests need
    // to set it externally (e.g. RenderMode::PBVR) via VolumeCommandDispatcher.
    void setRenderMode(int mode) { renderMode_ = static_cast<RenderMode>(mode); }
    int  getRenderMode() const   { return static_cast<int>(renderMode_); }

private:
    World* world_ = nullptr;
    int*         pId_   = nullptr;
    int*         pDenseId_ = nullptr;
    std::function<void()> onRebuild_;
    SceneListPanel* scenePanel_ = nullptr;

    SparseVolumeRenderer* pointRenderer_ = nullptr;
    DenseVolumeRenderer*  denseRenderer_ = nullptr;
    VectorFieldRenderer*  lineRenderer_  = nullptr;
    Phantom::Volume::PBVRRenderer*   pbvrRenderer_  = nullptr;
    std::function<void()>   onCameraReset_;

    RenderMode renderMode_      = RenderMode::Points;
    bool       showVectorField_ = false;
    bool       showVolumeGrid_  = false;
    float      pointSize_       = 4.0f;
    bool       showDensePoints_ = false;
    float      densePointSize_  = 4.0f;
    int        denseColorMap_   = 0;
    float      densityScale_         = 1.0f;
    float      particleSize_         = 4.0f;
    int        repeatCount_          = 1;
    bool       pbvrUseGPU_           = false;
    int        pbvrMaxParticlesPerVoxel_ = 4;

    // Self-shadow (experimental).
    bool       shadowEnabled_        = false;
    float      lightAzimuth_         = 45.0f;
    float      lightElevation_       = 60.0f;
    float      sigma_                = 1.0f;
    int        shadowLayers_         = 8;
    int        shadowSizeIdx_        = 1; // index into kShadowSizes
    int        pbvrTFPreset_         = 0;

    std::unique_ptr<IVolumeProcessView> activeProcessView_;
    bool activeProcessUsesDenseId_ = false;

    void syncRendererStates();
    void drawRenderSettings();
    void drawProcessView();
};

} // namespace VkVolumeView
