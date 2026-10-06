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

// ============================================================
//  Command routing
// ============================================================

std::string CommandDispatcher::route(const std::string& cmd) {
    if (cmd == "CheckCommandCatalog") return cmdCheckCommandCatalog();

    // --- Simple query / action commands ---

    if (cmd == "GetSceneCount") {
        if (!world_) return "Error:no world";
        return "SceneCount:" + std::to_string(world_->getScenes().size());
    }

    if (cmd == "GetDenseSceneCount") {
        if (!world_) return "Error:no world";
        return "DenseSceneCount:" + std::to_string(world_->getDenseScenes().size());
    }

    if (cmd == "GetTotalVoxelCount") {
        if (!world_) return "Error:no world";
        int total = 0;
        for (const auto& s : world_->getScenes())
            if (s->getShape()) total += s->getShape()->getActiveVoxelCount();
        return "TotalVoxelCount:" + std::to_string(total);
    }

    if (cmd == "GetVoxelCount") {
        if (!world_ || !pActiveSceneId_) return "Error:not initialized";
        const auto* scene = world_->findById(*pActiveSceneId_);
        if (!scene || !scene->getShape()) return "VoxelCount:0";
        return "VoxelCount:" + std::to_string(scene->getShape()->getActiveVoxelCount());
    }

    if (cmd == "GetSceneName") {
        if (!world_ || !pActiveSceneId_) return "Error:not initialized";
        const auto* scene = world_->findById(*pActiveSceneId_);
        if (!scene) return "Error:no active scene";
        return "SceneName:" + scene->getName();
    }

    if (cmd == "ClearWorld") {
        if (!world_) return "Error:no world";
        world_->clear();
        if (pActiveSceneId_) *pActiveSceneId_ = -1;
        if (pActiveDenseSceneId_) *pActiveDenseSceneId_ = -1;
        if (onRebuild_) onRebuild_();
        return "OK";
    }

    if (cmd == "GetPolygonCount") {
        if (!world_) return "Error:no world";
        return "PolygonCount:" + std::to_string(world_->getPolygons().size());
    }

    if (cmd == "ResetCamera") {
        if (onCameraReset_) onCameraReset_();
        return "OK";
    }

    if (cmd == "GetCameraDistance") {
        if (!pointRenderer_) return "Error:no renderer";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%f", pointRenderer_->getCameraState().distance);
        return buf;
    }

    if (cmd == "GetPBVRRepeatCount") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return "RepeatCount:" + std::to_string(pbvrRenderer_->getRepeatCount());
    }
    if (cmd == "GetVoxelPointSize" || cmd == "GetDensePointSize") {
        const bool dense = cmd == "GetDensePointSize";
        if (dense ? !denseRenderer_ : !pointRenderer_) return "Error:no renderer";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "PointSize:%g", dense ? denseRenderer_->getPointSize() : pointRenderer_->getPointSize());
        return buf;
    }

    if (cmd == "GetPBVRShadowEnabled") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return std::string("ShadowEnabled:") + (pbvrRenderer_->isShadowEnabled() ? "1" : "0");
    }

    // --- Pixel readback: response is deferred to the frame after the swap-chain copy ---

    if (cmd.rfind("GetPixelColor:", 0) == 0 || cmd.rfind("GetPixelBrightness:", 0) == 0) {
        const bool brightness = cmd.rfind("GetPixelBrightness:", 0) == 0;
        const std::string rest = cmd.substr(cmd.find(':') + 1);
        const auto p = splitBy(rest, ',');
        if (p.size() != 2) return "Error:expected x,y";
        int x, y;
        if (!parseInt(p[0], x) || !parseInt(p[1], y) || x < 0 || y < 0)
            return "Error:invalid coordinates";
        return brightness ? cmdGetPixelBrightness(static_cast<uint32_t>(x), static_cast<uint32_t>(y))
                           : cmdGetPixelColor(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    }

    // --- Path-based commands: extract everything after the first colon ---

    static constexpr std::string_view kScreenshot = "Screenshot:";
    if (cmd.rfind(kScreenshot, 0) == 0) {
        if (!app_) return "Error:no app";
        const std::string path = cmd.substr(kScreenshot.size());
        app_->requestScreenshot(path);
        return "OK:" + path;
    }

    static constexpr std::string_view kSetShowVolumeGrid = "SetShowVolumeGrid:";
    if (cmd.rfind(kSetShowVolumeGrid, 0) == 0) {
        if (!menuPanel_) return "Error:no menu panel";
        const std::string val = cmd.substr(kSetShowVolumeGrid.size());
        menuPanel_->setShowVolumeGrid(val != "0");
        return "OK";
    }

    static constexpr std::string_view kSetShowVectorField = "SetShowVectorField:";
    if (cmd.rfind(kSetShowVectorField, 0) == 0) {
        if (!menuPanel_) return "Error:no menu panel";
        const std::string val = cmd.substr(kSetShowVectorField.size());
        menuPanel_->setShowVectorField(val != "0");
        return "OK";
    }

    // Path argument may contain ':' (drive letters), so match by prefix.
    static constexpr std::string_view kDumpRadiance = "DumpPBVRParticleRadiance:";
    if (cmd.rfind(kDumpRadiance, 0) == 0) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return pbvrRenderer_->dumpParticleRadiance(std::string(cmd.substr(kDumpRadiance.size())))
            ? "OK" : "Error:no scattering result to dump";
    }

    // --- Parametric commands: split entire string by ':' ---

    const auto parts = splitBy(cmd, ':');
    if (parts.empty()) return "Error:empty command";

    if (parts[0] == "SetActiveScene" && parts.size() == 2) {
        if (!world_ || !pActiveSceneId_) return "Error:not initialized";
        int idx = 0;
        if (!parseInt(parts[1], idx)) return "Error:bad index";
        const auto& scenes = world_->getScenes();
        if (idx < 0 || idx >= static_cast<int>(scenes.size()))
            return "Error:index out of range";
        *pActiveSceneId_ = scenes[static_cast<size_t>(idx)]->getId();
        return "OK";
    }

    if (parts[0] == "SetActiveDenseScene" && parts.size() == 2) {
        if (!world_ || !pActiveDenseSceneId_) return "Error:not initialized";
        int idx = 0;
        if (!parseInt(parts[1], idx)) return "Error:bad index";
        const auto& scenes = world_->getDenseScenes();
        if (idx < 0 || idx >= static_cast<int>(scenes.size()))
            return "Error:index out of range";
        *pActiveDenseSceneId_ = scenes[static_cast<size_t>(idx)]->getId();
        return "OK";
    }

    if (parts[0] == "CreateSphere" && parts.size() == 6) {
        float f[5];
        if (!parseFloats(parts, 1, 5, f)) return "Error:bad CreateSphere params";
        return cmdCreateSphere(f[0], f[1], f[2], f[3], f[4]);
    }

    if (parts[0] == "CreateBox" && parts.size() == 8) {
        float f[7];
        if (!parseFloats(parts, 1, 7, f)) return "Error:bad CreateBox params";
        return cmdCreateBox(f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
    }

    if (parts[0] == "CSGCombine" && parts.size() == 4) {
        int idxA, idxB;
        if (!parseInt(parts[2], idxA) || !parseInt(parts[3], idxB))
            return "Error:bad CSGCombine indices";
        return cmdCsgCombine(parts[1], idxA, idxB);
    }

    if (parts[0] == "Resample" && parts.size() == 2) {
        float newCell;
        if (!parseFloat(parts[1], newCell)) return "Error:bad Resample cell size";
        return cmdResample(newCell);
    }

    if (parts[0] == "MarchingCubes" && parts.size() == 2) {
        float isoLevel;
        if (!parseFloat(parts[1], isoLevel)) return "Error:bad MarchingCubes isoLevel";
        return cmdMarchingCubes(isoLevel);
    }

    if (parts[0] == "CreateDenseBox" && parts.size() == 10) {
        float f[6];
        int   n[3];
        if (!parseFloats(parts, 1, 6, f) || !parseInts(parts, 7, 3, n))
            return "Error:bad CreateDenseBox params";
        return cmdCreateDenseBox(f[0], f[1], f[2], f[3], f[4], f[5], n[0], n[1], n[2]);
    }

    if (parts[0] == "DenseFromSparse" && parts.size() == 2) {
        float voxelSize;
        if (!parseFloat(parts[1], voxelSize)) return "Error:bad DenseFromSparse voxelSize";
        return cmdDenseFromSparse(voxelSize);
    }

    if (parts[0] == "DenseMarchingCubes" && parts.size() == 2) {
        float isoLevel;
        if (!parseFloat(parts[1], isoLevel)) return "Error:bad DenseMarchingCubes isoLevel";
        return cmdDenseMarchingCubes(isoLevel);
    }

    if (parts[0] == "DeleteDense" && parts.size() == 2) {
        int id = -1;
        if (!parseInt(parts[1], id)) return "Error:bad DeleteDense id";
        return cmdDeleteDense(id);
    }

    if (auto r = routePbvr(cmd, parts)) return *r;

    if (parts[0] == "SetPBVRRenderMode" && parts.size() == 2) {
        if (!menuPanel_) return "Error:no menu panel";
        int mode = 0;
        if (!parseInt(parts[1], mode) || mode < 0 || mode > 2) return "Error:bad SetPBVRRenderMode value";
        menuPanel_->setRenderMode(mode);
        return "OK";
    }

    if (parts[0] == "SetVoxelPointSize" || parts[0] == "SetDensePointSize" || parts[0] == "SetDensePoints"
        || parts[0] == "SetDenseColorMap") {
        if (parts.size() != 2) return "Error:bad " + parts[0] + " value";
        if (!menuPanel_) return "Error:no menu panel";
        float f = 0.0f;
        if (!parseFloat(parts[1], f)) return "Error:bad " + parts[0] + " value";
        if (parts[0] == "SetVoxelPointSize") menuPanel_->setVoxelPointSize(f);
        else if (parts[0] == "SetDensePointSize") menuPanel_->setDensePointSize(f);
        else if (parts[0] == "SetDensePoints") menuPanel_->setShowDensePoints(parts[1] != "0");
        else {
            if (f < 0.0f || f > 2.0f) return "Error:bad SetDenseColorMap value";
            menuPanel_->setDenseColorMap(static_cast<int>(f));
        }
        return "OK";
    }

    return "Error:unknown command '" + cmd + "'";
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
