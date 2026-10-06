#pragma once

#include "../../VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "../../VkAppBase/ScenarioRunner/UiCommand.h"
#include "../../VkAppBase/ScenarioRunner/CommandQueue.h"
#include "World.h"
#include "SparseVolumeRenderer.h"
#include "DenseVolumeRenderer.h"
#include "VectorFieldRenderer.h"
#include "MenuPanel.h"

#include <functional>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace VKG { class VkAppBase; }
namespace Phantom::Volume { class PBVRRenderer; }

namespace VolumeView {

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
    std::vector<CommandInfo> commandCatalog() const override;

    // GUI operations take the same queue and handlers as typed commands; the
    // response is discarded (see UiCommand.h).
    void submitUi(const std::string& cmd) { dispatch(markUiCommand(cmd)); }

private:
    std::string route(const std::string& cmd);
    // Menu setters share one body (CommandRoutes.cpp); `apply` returns false for a rejected value.
    using MenuApply = bool (*)(MenuPanel&, float, const std::string& rawText);
    std::string setMenuValue(const std::vector<std::string>& parts, MenuApply apply);
    static const char* parsePixelXY(const std::string& text, uint32_t& x, uint32_t& y);
    // PBVR renderer commands (CommandPbvr.cpp); nullopt = not one of them.
    std::optional<std::string> routePbvr(const std::string& cmd, const std::vector<std::string>& parts);
    std::string cmdCheckCommandCatalog();

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

    CommandQueue queue_;
};

} // namespace VolumeView
