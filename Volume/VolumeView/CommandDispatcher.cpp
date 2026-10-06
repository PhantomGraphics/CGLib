#include "CommandDispatcher.h"
#include "CommandParse.h"

#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Coord.h"
#include "../Volume/Volume.h"
#include "../VolumeRenderer/PBVRRenderer.h"
#include "../../VkAppBase/VkAppBase.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <functional>
#include <string_view>
#include <vector>

namespace VolumeView {

// ============================================================
//  IScenarioDispatcher interface
// ============================================================

void CommandDispatcher::dispatch(const std::string& command) {
    queue_.submit(command);
}

std::vector<std::string> CommandDispatcher::collectResponses() {
    return queue_.collectResponses();
}

std::vector<CommandInfo> CommandDispatcher::commandCatalog() const {
    return {
        {"GetSceneCount", "", ""},
        {"GetDenseSceneCount", "", ""},
        {"GetTotalVoxelCount", "", ""},
        {"GetVoxelCount", "", "Active sparse scene"},
        {"GetSceneName", "", ""},
        {"ClearWorld", "", ""},
        {"GetPolygonCount", "", ""},
        {"ResetCamera", "", ""},
        {"GetCameraDistance", "", ""},
        {"SetActiveScene", "index", "Select the active sparse scene (by list index)"},
        {"SetActiveDenseScene", "index", "Select the active dense scene (by list index)"},
        {"CreateSphere", "cx:cy:cz:radius:cell", "Sparse SDF sphere"},
        {"CreateBox", "minX:minY:minZ:maxX:maxY:maxZ:cell", "Sparse SDF box"},
        {"CSGCombine", "op:idxA:idxB", "op = Union|Intersection|Difference"},
        {"Resample", "cell", ""},
        {"MarchingCubes", "isoLevel", ""},
        {"CreateDenseBox", "minX:minY:minZ:maxX:maxY:maxZ:resX:resY:resZ", ""},
        {"DenseFromSparse", "voxelSize", ""},
        {"DenseMarchingCubes", "isoLevel", ""},
        {"DeleteDense", "id", ""},
        {"SetShowVolumeGrid", "0|1", ""},
        {"SetShowVectorField", "0|1", ""},
        {"Screenshot", "path", "Save a PNG"},
        {"GetPixelColor", "x,y", "Deferred answer (next frame)"},
        {"GetPixelBrightness", "x,y", "Deferred answer (next frame)"},
        {"DumpPBVRParticleRadiance", "path", ""},
        {"SetPBVRRenderMode", "0|1|2", "Points|PBVR|Both"},
        {"SetPBVRUseGPU", "0|1", ""},
        {"SetPBVRParticleSize", "float", ""},
        {"SetPBVRDensityScale", "float", ""},
        {"SetPBVRCameraDistance", "float", ""},
        {"SetPBVRCameraTarget", "x:y:z", ""},
        {"SetPBVRCameraFov", "float", ""},
        {"SetPBVRCameraAngles", "azimuth:elevation", ""},
        {"SetPBVRMultipleScattering", "0|1", ""},
        {"SetPBVRScatteringOrders", "int", ""},
        {"SetPBVRProbeCount", "int", ""},
        {"SetPBVRProbeRadius", "float", ""},
        {"SetPBVRPhaseG", "float", ""},
        {"SetPBVRScatteringAlbedo", "float", ""},
        {"SetPBVRProbeSourceBudget", "int", ""},
        {"SetPBVRScatteringSHDegree", "int", ""},
        {"SetPBVRScatteringExposure", "float", ""},
        {"SetPBVRTFPreset", "int", ""},
        {"SetPBVRShadowEnabled", "0|1", ""},
        {"GetPBVRShadowEnabled", "", ""},
        {"SetPBVRLightDir", "azimuth:elevation", ""},
        {"SetPBVRExtinction", "float", ""},
        {"SetPBVRShadowLayers", "int", ""},
        {"SetPBVRShadowMapSize", "int", ""},
        {"SetPBVRRepeatCount", "int", ""},
        {"SetPBVRMaxParticlesPerVoxel", "int", "GPU generation cap"},
        {"GetPBVRRepeatCount", "", ""},
        {"GetVoxelPointSize", "", ""},
        {"GetDensePointSize", "", ""},
        {"SetVoxelPointSize", "float", "Voxel point size"},
        {"SetDensePoints", "0|1", "Show dense volume points"},
        {"SetDensePointSize", "float", ""},
        {"SetDenseColorMap", "0|1|2", "Jet|Viridis|Grayscale"},
        {"GetPBVRMeanScatteredRadiance", "", ""},
        {"GetPBVRMeanIndirectRadiance", "", ""},
        {"GetPBVRMeanSunTransmittance", "", ""},
        {"GetPBVRScatteringSolveMs", "", ""},
    };
}

std::string CommandDispatcher::cmdCheckCommandCatalog() {
    // Probe with junk arguments of the right arity ("name:x:x..."): a routed name
    // answers with its own validation error, only an unrouted one says "unknown
    // command". Every collaborator is detached (and the world swapped for a scratch
    // one), so a probe cannot touch real state. Screenshot / Dump are skipped:
    // they write files.
    World scratch;
    World* const sWorld = world_;
    int* const sActive = pActiveSceneId_;
    int* const sDense = pActiveDenseSceneId_;
    auto sRebuild = std::move(onRebuild_);
    auto sCameraReset = std::move(onCameraReset_);
    SparseVolumeRenderer* const sPoint = pointRenderer_;
    DenseVolumeRenderer* const sDenseR = denseRenderer_;
    VectorFieldRenderer* const sLine = lineRenderer_;
    MenuPanel* const sMenu = menuPanel_;
    Phantom::Volume::PBVRRenderer* const sPbvr = pbvrRenderer_;
    ::VKG::VkAppBase* const sApp = app_;
    world_ = &scratch; pActiveSceneId_ = nullptr; pActiveDenseSceneId_ = nullptr;
    onRebuild_ = nullptr; onCameraReset_ = nullptr;
    pointRenderer_ = nullptr; denseRenderer_ = nullptr; lineRenderer_ = nullptr;
    menuPanel_ = nullptr; pbvrRenderer_ = nullptr; app_ = nullptr;

    std::string missing;
    for (const auto& c : commandCatalog()) {
        if (c.name == "Screenshot" || c.name == "DumpPBVRParticleRadiance") continue;
        std::string probe = c.name;
        if (!c.args.empty()) {
            const bool comma = c.args.find(',') != std::string::npos && c.args.find(':') == std::string::npos;
            size_t tokens = 1;
            for (char ch : c.args) if (ch == ':') ++tokens;
            probe += ":";
            for (size_t i = 0; i < tokens; ++i) probe += (i ? (comma ? "," : ":") : "") + std::string("x");
            if (comma) probe = c.name + ":x,x";
        }
        if (route(probe).rfind("Error:unknown command", 0) == 0)
            missing += (missing.empty() ? "" : ",") + c.name;
    }

    world_ = sWorld; pActiveSceneId_ = sActive; pActiveDenseSceneId_ = sDense;
    onRebuild_ = std::move(sRebuild); onCameraReset_ = std::move(sCameraReset);
    pointRenderer_ = sPoint; denseRenderer_ = sDenseR; lineRenderer_ = sLine;
    menuPanel_ = sMenu; pbvrRenderer_ = sPbvr; app_ = sApp;
    return missing.empty() ? "OK" : "Error:unrouted catalog entries: " + missing;
}

void CommandDispatcher::processQueue() {
    // If a pixel read is pending, check whether the result is available before processing
    // any new commands from the input queue.
    if (pixelReadPending_) {
        uint8_t data[4];
        if (app_ && app_->pollPixelRead(data)) {
            std::string resp;
            if (pixelBrightness_) {
                char buf[32];
                const float lum = (data[0] * 0.299f + data[1] * 0.587f + data[2] * 0.114f) / 255.f;
                std::snprintf(buf, sizeof(buf), "%.4f", lum);
                resp = buf;
            } else {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "0x%02X%02X%02X",
                    static_cast<unsigned>(data[0]), static_cast<unsigned>(data[1]), static_cast<unsigned>(data[2]));
                resp = buf;
            }
            queue_.respond(std::move(resp));
            pixelReadPending_ = false;
            pixelBrightness_  = false;
        }
        return; // Don't consume new commands until the read completes.
    }

    std::queue<std::string> local = queue_.takeAll();
    while (!local.empty()) {
        std::string cmd = std::move(local.front());
        local.pop();
        const bool fromUi = takeUiMark(cmd);
        std::string resp = route(cmd);
        if (fromUi) continue;

        if (pixelReadPending_) {
            // GetPixelColor/GetPixelBrightness was just dispatched; re-queue any remaining
            // commands so they are processed only after the pixel read resolves (see the
            // pixelReadPending_ branch above), keeping responses in 1:1 order with commands.
            queue_.requeue(local);
            break;
        }

        queue_.respond(std::move(resp));
    }
}

std::string CommandDispatcher::cmdGetPixelColor(uint32_t x, uint32_t y) {
    if (!app_) return "Error:app not available";
    app_->requestPixelRead(x, y);
    pixelReadPending_ = true;
    pixelBrightness_  = false;
    return {}; // response is deferred to the next frame
}

std::string CommandDispatcher::cmdGetPixelBrightness(uint32_t x, uint32_t y) {
    if (!app_) return "Error:app not available";
    app_->requestPixelRead(x, y);
    pixelReadPending_ = true;
    pixelBrightness_  = true;
    return {}; // response is deferred to the next frame
}

} // namespace VolumeView
