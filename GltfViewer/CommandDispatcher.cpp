#include "CommandDispatcher.h"
#include <cstdio>
#include <cmath>
#include "App.h"

#include <charconv>
#include <string>

using namespace Phantom::Gltf;

void CommandDispatcher::dispatch(const std::string& command) {
    queue_.submit(command);
}

std::vector<std::string> CommandDispatcher::collectResponses() {
    return queue_.collectResponses();
}

namespace {
// "a:b:c" -> exactly `n` finite floats.
bool parseColonFloats(const std::string& text, float* out, int n) {
    size_t pos = 0;
    for (int i = 0; i < n; ++i) {
        const size_t end = (i == n - 1) ? text.size() : text.find(':', pos);
        if (end == std::string::npos || end < pos) return false;
        const auto r = std::from_chars(text.data() + pos, text.data() + end, out[i]);
        if (r.ec != std::errc{} || r.ptr != text.data() + end || !std::isfinite(out[i])) return false;
        pos = end + 1;
    }
    return true;
}
} // namespace

std::vector<CommandInfo> CommandDispatcher::commandCatalog() const {
    return {
        {"GetStatus", "", ""},
        {"GetMeshCount", "", ""},
        {"GetNodeCount", "", ""},
        {"GetMaterialCount", "", ""},
        {"GetTextureCount", "", ""},
        {"GetSceneCount", "", ""},
        {"GetSkinCount", "", ""},
        {"GetPrimitiveCount", "", ""},
        {"GetCamDist", "", ""},
        {"SetCamDist", "float", "Orbit camera distance"},
        {"SetCamTarget", "x:y:z", "Orbit camera target"},
        {"SetLight", "x:y:z:r:g:b:intensity", "Directional light position, color and intensity"},
        {"GetLight", "", "x:y:z:intensity"},
        {"ResetCamera", "", ""},
        {"GetHasAssetCamera", "", ""},
        {"GetUseAssetCamera", "", ""},
        {"SetUseAssetCamera", "0|1", ""},
        {"GetUseIBL", "", ""},
        {"SetUseIBL", "0|1", ""},
        {"GetUseSkybox", "", ""},
        {"SetUseSkybox", "0|1", ""},
        {"LoadEnvironmentHDR", "path", ""},
        {"ClearEnvironmentHDR", "", ""},
        {"GetHasEnvironmentHDR", "", ""},
        {"LoadPhmat", "materialIndex,path", "Apply a .phmat shader override"},
        {"ClearPhmat", "materialIndex", ""},
        {"ShowShaderGraph", "materialIndex", "Open the Shader Graph panel"},
        {"GetHasPhmatOverride", "materialIndex", ""},
        {"GetPhmatPipelineVariantCount", "", ""},
        {"SetPhmatHotReload", "0|1", ""},
        {"GetPhmatHotReload", "", ""},
        {"LoadFile", "path", "Load a glTF/GLB/VRM/OBJ/STL file (response arrives when loaded)"},
        {"SaveScreenshot", "path", "Save a PNG (response arrives when written)"},
        {"GetVrmSpecVersion", "", ""},
        {"GetVrmHumanBoneCount", "", ""},
        {"GetVrmExpressionCount", "", ""},
        {"GetVrmMetaTitle", "", ""},
        {"SetVrmExpressionWeight", "index:weight", ""},
    };
}

std::string CommandDispatcher::cmdCheckCommandCatalog() {
    // Probe with a junk argument: a routed name answers with its own validation
    // error, only an unrouted one says "unknown command". LoadFile/SaveScreenshot
    // are skipped (they queue work and answer later); scenarios cover them.
    std::string missing;
    for (const auto& c : commandCatalog()) {
        if (c.name == "LoadFile" || c.name == "SaveScreenshot") continue;
        const std::string probe = c.args.empty() ? c.name : c.name + ":x";
        if (route(probe).rfind("Error:unknown command", 0) == 0)
            missing += (missing.empty() ? "" : ",") + c.name;
    }
    return missing.empty() ? "OK" : "Error:unrouted catalog entries: " + missing;
}

void CommandDispatcher::processQueue() {
    std::queue<std::string> local = queue_.takeAll();

    while (!local.empty()) {
        std::string cmd = std::move(local.front());
        local.pop();
        const bool fromUi = takeUiMark(cmd);
        std::string resp = route(cmd);
        if (!resp.empty() && !fromUi) queue_.respond(std::move(resp));
    }
}

std::optional<std::filesystem::path> CommandDispatcher::takePendingLoad() {
    std::optional<std::filesystem::path> p;
    p.swap(pendingLoad_);
    return p;
}

std::optional<std::filesystem::path> CommandDispatcher::takePendingScreenshot() {
    std::optional<std::filesystem::path> p;
    p.swap(pendingScreenshot_);
    return p;
}

void CommandDispatcher::signalScreenshotDone(bool ok, const std::string& path) {
    queue_.respond(ok ? "OK:saved " + path : "Error:screenshot failed");
}

void CommandDispatcher::signalLoaded(bool ok, const std::string& msg) {
    if (ok) {
        queue_.respond("OK:" + (msg.empty() ? std::string("loaded") : msg));
    } else {
        queue_.respond("Error:" + msg);
    }
}

std::string CommandDispatcher::route(const std::string& cmd) {
    if (cmd == "CheckCommandCatalog") return cmdCheckCommandCatalog();
    if (cmd == "GetStatus") {
        return "OK";
    }

    if (cmd == "GetMeshCount") {
        if (!doc_) return "MeshCount:0";
        return "MeshCount:" + std::to_string(doc_->meshes.size());
    }

    if (cmd == "GetNodeCount") {
        if (!doc_) return "NodeCount:0";
        return "NodeCount:" + std::to_string(doc_->nodes.size());
    }

    if (cmd == "GetMaterialCount") {
        if (!doc_) return "MaterialCount:0";
        return "MaterialCount:" + std::to_string(doc_->materials.size());
    }

    if (cmd == "GetTextureCount") {
        if (!doc_) return "TextureCount:0";
        return "TextureCount:" + std::to_string(doc_->textures.size());
    }

    if (cmd == "GetSceneCount") {
        if (!doc_) return "SceneCount:0";
        return "SceneCount:" + std::to_string(doc_->scenes.size());
    }

    if (cmd == "GetSkinCount") {
        if (!doc_) return "SkinCount:0";
        return "SkinCount:" + std::to_string(doc_->skins.size());
    }

    if (cmd == "GetPrimitiveCount") {
        if (!doc_) return "PrimitiveCount:0";
        size_t total = 0;
        for (const auto& mesh : doc_->meshes)
            total += mesh.primitives.size();
        return "PrimitiveCount:" + std::to_string(total);
    }

    if (cmd == "GetCamDist") {
        if (!renderer_) return "Val:0";
        return "Val:" + std::to_string(static_cast<int>(*renderer_->camDistPtr()));
    }

    if (cmd.rfind("SetCamTarget:", 0) == 0) {
        if (!renderer_) return "Error:no renderer";
        float v[3];
        if (!parseColonFloats(cmd.substr(13), v, 3)) return "Error:expected SetCamTarget:x:y:z";
        *renderer_->camTargetPtr() = { v[0], v[1], v[2] };
        return "OK";
    }

    if (cmd.rfind("SetLight:", 0) == 0) {
        if (!app_) return "Error:no app";
        float v[7];
        if (!parseColonFloats(cmd.substr(9), v, 7)) return "Error:expected SetLight:x:y:z:r:g:b:intensity";
        if (v[6] < 0.f) return "Error:intensity must be >= 0";
        app_->setLight({ v[0], v[1], v[2] }, { v[3], v[4], v[5] }, v[6]);
        return "OK";
    }

    if (cmd == "GetLight") {
        if (!app_) return "Error:no app";
        const auto& vp = app_->viewPanel();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%g:%g:%g:%g", vp.lightPos().x, vp.lightPos().y, vp.lightPos().z, vp.lightIntensity());
        return buf;
    }

    if (cmd.rfind("SetCamDist:", 0) == 0) {
        if (!renderer_) return "Error:no renderer";
        const std::string value = cmd.substr(11);
        float dist = 0.f;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), dist);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            return "Error:invalid SetCamDist value";
        *renderer_->camDistPtr() = dist;
        return "OK";
    }

    // Camera-as-scene-component (2026-09-16, App::hasAssetCamera()'s comment): opt-in, off by
    // default, same three-command shape as RayTracer's SetUseAssetCamera/GetHasAssetCamera.
    if (cmd == "GetHasAssetCamera") {
        if (!app_) return "Val:0";
        return "Val:" + std::to_string(app_->hasAssetCamera() ? 1 : 0);
    }
    if (cmd == "GetUseAssetCamera") {
        if (!app_) return "Val:0";
        return "Val:" + std::to_string(app_->useAssetCamera() ? 1 : 0);
    }
    if (cmd.rfind("SetUseAssetCamera:", 0) == 0) {
        if (!app_) return "Error:no app";
        if (cmd.substr(18) != "0" && !app_->hasAssetCamera()) return "Error:no asset camera";
        app_->setUseAssetCamera(cmd.substr(18) != "0");
        return "OK";
    }

    if (cmd == "GetUseIBL") {
        if (!renderer_) return "Val:0";
        return "Val:" + std::to_string(renderer_->getUseIBL());
    }

    if (cmd.rfind("SetUseIBL:", 0) == 0) {
        if (!renderer_) return "Error:no renderer";
        renderer_->setUseIBL(cmd.substr(10) != "0");
        return "OK";
    }

    if (cmd == "GetUseSkybox") {
        if (!renderer_) return "Val:0";
        return "Val:" + std::to_string(renderer_->getUseSkybox() ? 1 : 0);
    }

    if (cmd.rfind("SetUseSkybox:", 0) == 0) {
        if (!renderer_) return "Error:no renderer";
        if (!renderer_->hasSkyboxPipeline()) return "Error:no skybox shaders loaded";
        renderer_->setUseSkybox(cmd.substr(13) != "0");
        return "OK";
    }

    // Real HDRI loading (2026-09-15): replaces the placeholder tint cubemap with a real
    // equirectangular .hdr panorama (see App::loadEnvironmentHDR()'s comment).
    if (cmd.rfind("LoadEnvironmentHDR:", 0) == 0) {
        if (!app_) return "Error:no app";
        if (!app_->loadEnvironmentHDR(cmd.substr(19))) return "Error:failed to load HDRI";
        return "OK";
    }
    if (cmd == "ClearEnvironmentHDR") {
        if (!app_) return "Error:no app";
        app_->clearEnvironmentHDR();
        return "OK";
    }
    if (cmd == "GetHasEnvironmentHDR") {
        if (!app_) return "Val:0";
        return "Val:" + std::to_string(app_->hasEnvironmentHDR());
    }

    // .phmat shader graph material override (Phase 4C, see App::loadPhmatMaterial()'s comment).
    // "LoadPhmat:<materialIndex>,<path>" -- split on the FIRST comma only, since a Windows path
    // itself contains colons ("C:\...") but materialIndex never contains a comma.
    if (cmd.rfind("LoadPhmat:", 0) == 0) {
        if (!app_) return "Error:no app";
        const std::string args = cmd.substr(10);
        const auto sep = args.find(',');
        if (sep == std::string::npos) return "Error:expected LoadPhmat:<materialIndex>,<path>";
        int materialIndex = -1;
        const std::string idxStr = args.substr(0, sep);
        const auto idxResult = std::from_chars(idxStr.data(), idxStr.data() + idxStr.size(), materialIndex);
        if (idxResult.ec != std::errc{}) return "Error:invalid materialIndex";
        std::string err;
        if (!app_->loadPhmatMaterial(materialIndex, args.substr(sep + 1), &err))
            return "Error:" + err;
        return "OK";
    }
    if (cmd.rfind("ShowShaderGraph:", 0) == 0) {
        if (!app_) return "Error:no app";
        int materialIndex = -1;
        const std::string value = cmd.substr(16);
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), materialIndex);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || materialIndex < 0)
            return "Error:invalid materialIndex";
        app_->showShaderGraph(materialIndex);
        return "OK";
    }
    if (cmd.rfind("ClearPhmat:", 0) == 0) {
        if (!app_) return "Error:no app";
        int materialIndex = -1;
        const std::string idxStr = cmd.substr(11);
        const auto idxResult = std::from_chars(idxStr.data(), idxStr.data() + idxStr.size(), materialIndex);
        if (idxResult.ec != std::errc{}) return "Error:invalid materialIndex";
        app_->clearPhmatMaterial(materialIndex);
        return "OK";
    }
    if (cmd.rfind("GetHasPhmatOverride:", 0) == 0) {
        if (!app_) return "Val:0";
        int materialIndex = -1;
        const std::string idxStr = cmd.substr(20);
        const auto idxResult = std::from_chars(idxStr.data(), idxStr.data() + idxStr.size(), materialIndex);
        if (idxResult.ec != std::errc{}) return "Error:invalid materialIndex";
        return "Val:" + std::to_string(app_->hasPhmatOverride(materialIndex) ? 1 : 0);
    }
    // Phase 4C item 5: distinct VkPipeline objects behind every .phmat override applied so far in
    // this document -- see App::phmatPipelineVariantCount()'s comment ("shader variant" reuse).
    if (cmd == "GetPhmatPipelineVariantCount") {
        if (!app_) return "Val:0";
        return "Val:" + std::to_string(app_->phmatPipelineVariantCount());
    }
    // Phase 4C item 5 ("hot reload"), opt-in and off by default -- see App::phmatHotReloadEnabled()'s comment.
    if (cmd.rfind("SetPhmatHotReload:", 0) == 0) {
        if (!app_) return "Error:no app";
        app_->setPhmatHotReloadEnabled(cmd.substr(18) == "1");
        return "OK";
    }
    if (cmd == "GetPhmatHotReload") {
        if (!app_) return "Val:0";
        return "Val:" + std::to_string(app_->phmatHotReloadEnabled() ? 1 : 0);
    }

    if (cmd == "ResetCamera") {
        if (!renderer_) return "Error:no renderer";
        *renderer_->camDistPtr()   = 3.0f;
        *renderer_->camTargetPtr() = {0.f, 0.f, 0.f};
        return "OK";
    }

    if (cmd.rfind("LoadFile:", 0) == 0) {
        pendingLoad_ = std::filesystem::path(cmd.substr(9));
        return {};
    }

    if (cmd.rfind("SaveScreenshot:", 0) == 0) {
        pendingScreenshot_ = std::filesystem::path(cmd.substr(15));
        return {};
    }

    if (cmd == "GetVrmSpecVersion") {
        if (!app_) return "Val:-1";
        switch (app_->vrmState().specVersion) {
            case VrmSpecVersion::V0: return "Val:0";
            case VrmSpecVersion::V1: return "Val:1";
            default:                 return "Val:-1";
        }
    }

    if (cmd == "GetVrmHumanBoneCount") {
        if (!app_) return "Count:0";
        return "Count:" + std::to_string(app_->vrmState().humanoid.boneNameToNode.size());
    }

    if (cmd == "GetVrmExpressionCount") {
        if (!app_) return "Count:0";
        return "Count:" + std::to_string(app_->vrmState().expressions.size());
    }

    if (cmd == "GetVrmMetaTitle") {
        if (!app_) return "Val:";
        return "Val:" + app_->vrmState().meta.title;
    }

    static const std::string kSetVrmExpressionWeightPrefix = "SetVrmExpressionWeight:";
    if (cmd.rfind(kSetVrmExpressionWeightPrefix, 0) == 0) {
        if (!app_) return "Error:no app";
        const std::string args = cmd.substr(kSetVrmExpressionWeightPrefix.size());
        const auto sep = args.find(':');
        if (sep == std::string::npos)
            return "Error:expected SetVrmExpressionWeight:<index>:<weight>";

        const std::string idxStr = args.substr(0, sep);
        const std::string wStr   = args.substr(sep + 1);
        int   index  = -1;
        float weight = 0.f;
        const auto idxResult = std::from_chars(idxStr.data(), idxStr.data() + idxStr.size(), index);
        const auto wResult   = std::from_chars(wStr.data(), wStr.data() + wStr.size(), weight);
        if (idxResult.ec != std::errc{} || wResult.ec != std::errc{})
            return "Error:invalid SetVrmExpressionWeight arguments";
        if (index < 0 || index >= static_cast<int>(app_->vrmState().expressions.size()))
            return "Error:expression index out of range";

        app_->setVrmExpressionWeight(index, weight);
        return "OK";
    }

    return "Error:unknown command '" + cmd + "'";
}
