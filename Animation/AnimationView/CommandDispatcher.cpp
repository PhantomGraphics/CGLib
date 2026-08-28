#include "CommandDispatcher.h"

#include <charconv>
#include <cstdio>
#include <string_view>

namespace Phantom::Animation {

void CommandDispatcher::dispatch(const std::string& command)
{
    std::lock_guard<std::mutex> lock(mutex_);
    inputQueue_.push(command);
}

std::vector<std::string> CommandDispatcher::collectResponses()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    while (!outputQueue_.empty()) {
        out.push_back(outputQueue_.front());
        outputQueue_.pop();
    }
    return out;
}

void CommandDispatcher::processQueue()
{
    std::string cmd;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (inputQueue_.empty()) return;
        cmd = inputQueue_.front();
        inputQueue_.pop();
    }
    const std::string response = route(cmd);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        outputQueue_.push(response);
    }
}

std::string CommandDispatcher::route(const std::string& cmd)
{
    if (!world_) return "ERROR:NoWorld";

    const std::string_view sv(cmd);

    if (sv == "Play") {
        world_->playing = true;
        return "OK:Play";
    }
    if (sv == "Stop") {
        world_->playing = false;
        return "OK:Stop";
    }
    if (sv == "Reset") {
        world_->currentTime = 0.f;
        world_->playing     = false;
        world_->dirty       = true;
        return "OK:Reset";
    }
    if (sv == "GetTime") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Time:%.3f", world_->currentTime);
        return buf;
    }
    if (sv.starts_with("SetTime:")) {
        const auto valStr = sv.substr(8);
        float t = 0.f;
        auto [ptr, ec] = std::from_chars(valStr.data(), valStr.data() + valStr.size(), t);
        if (ec == std::errc{}) {
            world_->currentTime = t;
            world_->dirty       = true;
            return "OK:SetTime";
        }
        return "ERROR:BadValue";
    }
    if (sv.starts_with("SetSpeed:")) {
        const auto valStr = sv.substr(9);
        float s = 1.f;
        auto [ptr, ec] = std::from_chars(valStr.data(), valStr.data() + valStr.size(), s);
        if (ec == std::errc{}) {
            world_->speed = s;
            return "OK:SetSpeed";
        }
        return "ERROR:BadValue";
    }
    if (sv.starts_with("SetVisible:")) {
        const auto rest = sv.substr(11);
        const auto colon = rest.find(':');
        if (colon != std::string_view::npos) {
            const auto target = rest.substr(0, colon);
            const auto val    = rest.substr(colon + 1);
            bool v = (val == "1" || val == "true");
            if (target == "Bones") { world_->showBones = v; return "OK:SetVisible:Bones"; }
            if (target == "Mesh")  { world_->showMesh  = v; return "OK:SetVisible:Mesh"; }
        }
        return "ERROR:BadTarget";
    }
    if (sv.starts_with("WaitFrames:")) {
        return "OK:WaitFrames";
    }
    if (sv.starts_with("LoadPMX:")) {
        world_->loadedModelPath = std::string(sv.substr(8));
        return "OK:LoadPMX";
    }
    if (sv.starts_with("LoadVMD:")) {
        world_->loadedMotionPath = std::string(sv.substr(8));
        return "OK:LoadVMD";
    }
    if (sv.starts_with("SetIKEnabled:")) {
        // IK is already baked into the loaded document at load time (MmdAnimationBaker::bakeIk(),
        // via MmdToGltfConverter) -- this flag no longer changes the computed pose. Kept as a
        // no-op-but-OK command purely for scenario/UI compatibility (see AnimationWorld.h).
        const auto val = sv.substr(13);
        world_->ikEnabled = (val == "1" || val == "true");
        return "OK:SetIKEnabled";
    }
    if (sv == "GetIKCount") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "IKCount:%d", world_->ikCount);
        return buf;
    }
    if (sv == "GetMorphCount") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "MorphCount:%d", world_->morphCount);
        return buf;
    }
    if (sv == "GetBoneCount") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "BoneCount:%d", world_->boneCount);
        return buf;
    }
    if (sv == "GetVertCount") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "VertCount:%d", world_->vertCount);
        return buf;
    }
    if (sv == "GetSubMeshCount") {
        // One glTF primitive per PMX material submesh (see SkeletonGltfConverter) -- 0 before any
        // PMX has been loaded, since `document.meshes` itself starts out empty.
        const int count = world_->document.meshes.empty()
            ? 0 : (int)world_->document.meshes[0].primitives.size();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "SubMeshCount:%d", count);
        return buf;
    }

    return "ERROR:UnknownCommand:" + cmd;
}

} // namespace VKAnim
