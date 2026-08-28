#pragma once

#include "../../VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "World.h"
#include "SparseVolumeRenderer.h"
#include "DenseVolumeRenderer.h"
#include "VectorFieldRenderer.h"
#include "MenuPanel.h"

#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace VKG { class VkAppBase; }
namespace Phantom::Volume { class PBVRRenderer; }

namespace VkVolumeView {

class CommandDispatcher : public IScenarioDispatcher {
public:
    void setWorld(World* w)                    { world_          = w; }
    void setActiveSceneId(int* id)                   { pActiveSceneId_ = id; }
    void setActiveDenseSceneId(int* id)              { pActiveDenseSceneId_ = id; }
    void setOnRebuild(std::function<void()> cb)      { onRebuild_      = std::move(cb); }
    void setPointRenderer(SparseVolumeRenderer* r) { pointRenderer_  = r; }
    void setDenseRenderer(DenseVolumeRenderer* r)  { denseRenderer_  = r; }
    void setLineRenderer(VectorFieldRenderer* r)   { lineRenderer_   = r; }
    void setMenuPanel(MenuPanel* p)            { menuPanel_      = p; }
    void setOnCameraReset(std::function<void()> cb)  { onCameraReset_  = std::move(cb); }
    void setPbvrRenderer(Phantom::Volume::PBVRRenderer* r) { pbvrRenderer_ = r; }
    void setApp(::VKG::VkAppBase* app)               { app_            = app; }

    // Call once per frame from onUpdate() to drain the input queue on the render thread.
    void processQueue();

    void dispatch(const std::string& command) override;
    std::vector<std::string> collectResponses() override;

private:
    std::string route(const std::string& cmd);

    std::string cmdCreateSphere(float cx, float cy, float cz, float radius, float cell);
    std::string cmdCreateBox(float minX, float minY, float minZ,
                             float maxX, float maxY, float maxZ, float cell);
    std::string cmdCsgCombine(const std::string& op, int idxA, int idxB);
    std::string cmdResample(float newCell);
    std::string cmdMarchingCubes(float isoLevel);
    std::string cmdCreateDenseBox(float minX, float minY, float minZ,
                                  float maxX, float maxY, float maxZ,
                                  int resX, int resY, int resZ);
    std::string cmdDenseFromSparse(float voxelSize);
    std::string cmdDenseMarchingCubes(float isoLevel);
    std::string cmdDeleteDense(int id);
    std::string cmdGetPixelColor(uint32_t x, uint32_t y);
    std::string cmdGetPixelBrightness(uint32_t x, uint32_t y);
    std::string cmdExportVDB(const std::string& path);
    std::string cmdImportVDB(const std::string& path);

    World*            world_          = nullptr;
    int*                    pActiveSceneId_ = nullptr;
    int*                    pActiveDenseSceneId_ = nullptr;
    std::function<void()>   onRebuild_;
    SparseVolumeRenderer* pointRenderer_  = nullptr;
    DenseVolumeRenderer*  denseRenderer_  = nullptr;
    VectorFieldRenderer*  lineRenderer_   = nullptr;
    MenuPanel*        menuPanel_      = nullptr;
    std::function<void()>   onCameraReset_;
    Phantom::Volume::PBVRRenderer* pbvrRenderer_ = nullptr;
    ::VKG::VkAppBase*              app_          = nullptr;

    int opCount_ = 0;

    // GetPixelColor/GetPixelBrightness are resolved asynchronously: the response is deferred
    // until the swap-chain readback for the *next* frame completes (see processQueue()).
    bool pixelReadPending_ = false;
    bool pixelBrightness_  = false;

    std::mutex              mutex_;
    std::queue<std::string> inputQueue_;
    std::queue<std::string> outputQueue_;
};

} // namespace VkVolumeView
