#include "CommandDispatcher.h"
#include "App.h"

#include <charconv>
#include <string>

using namespace Phantom::Gltf;

void CommandDispatcher::dispatch(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputQueue_.push(command);
}

std::vector<std::string> CommandDispatcher::collectResponses() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    while (!outputQueue_.empty()) {
        out.push_back(std::move(outputQueue_.front()));
        outputQueue_.pop();
    }
    return out;
}

void CommandDispatcher::processQueue() {
    std::queue<std::string> local;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(local, inputQueue_);
    }

    while (!local.empty()) {
        std::string cmd = std::move(local.front());
        local.pop();
        std::string resp = route(cmd);
        if (!resp.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            outputQueue_.push(std::move(resp));
        }
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
    std::lock_guard<std::mutex> lock(mutex_);
    outputQueue_.push(ok ? "OK:saved " + path : "Error:screenshot failed");
}

void CommandDispatcher::signalLoaded(bool ok, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ok) {
        outputQueue_.push("OK:" + (msg.empty() ? std::string("loaded") : msg));
    } else {
        outputQueue_.push("Error:" + msg);
    }
}

std::string CommandDispatcher::route(const std::string& cmd) {
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

    if (cmd.rfind("SetCamDist:", 0) == 0) {
        if (!renderer_) return "Error:no renderer";
        *renderer_->camDistPtr() = std::stof(cmd.substr(11));
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
