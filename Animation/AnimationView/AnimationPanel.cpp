#include "AnimationPanel.h"

#include "imgui.h"
#include <cinttypes>

namespace Phantom::Animation {

void AnimationPanel::onImGui()
{
    if (!world_) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_FirstUseEver);

    ImGui::Begin("Animation");

    const float duration = world_->duration;

    if (ImGui::Button(world_->playing ? "Stop" : "Play")) {
        world_->playing = !world_->playing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        world_->currentTime = 0.f;
        world_->playing     = false;
        world_->dirty       = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &world_->loop);

    char label[64];
    std::snprintf(label, sizeof(label), "%.2f / %.2f s", world_->currentTime, duration);
    float t = (duration > 0.f) ? world_->currentTime / duration : 0.f;
    if (ImGui::SliderFloat("Time", &t, 0.f, 1.f, label)) {
        world_->currentTime = t * duration;
        world_->dirty       = true;
    }

    ImGui::SliderFloat("Speed", &world_->speed, 0.1f, 3.0f, "%.1fx");

    ImGui::Separator();
    ImGui::Checkbox("Show Mesh", &world_->showMesh);

    ImGui::Separator();
    ImGui::Text("Bones: %d  Verts: %d  IK: %d  Morphs: %d",
        world_->boneCount, world_->vertCount, world_->ikCount, world_->morphCount);

    // IK -- informational only post-migration (see AnimationWorld.h: baked at load time).
    if (world_->ikCount > 0) {
        ImGui::Separator();
        ImGui::Checkbox("IK Enabled", &world_->ikEnabled);
        ImGui::TextDisabled("(baked into the loaded animation; toggling has no live effect)");
    }

    // Morphs -- driven entirely by the loaded VMD's baked morph-weight animation now; no more
    // per-morph manual override sliders (there is nothing left in AnimationWorld to slide: the
    // weights are computed fresh every frame from world_->document via
    // GltfAnimationEvaluator::evaluateMorphWeights()).
    if (world_->morphCount > 0) {
        ImGui::Separator();
        ImGui::Text("%d morph target(s), driven by the loaded VMD's morph animation.", world_->morphCount);
    }

    ImGui::Separator();
    ImGui::Text("Load Model/Motion:");

    static char modelBuf[512] = "";
    static char motionBuf[512] = "";

    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("##model", modelBuf, sizeof(modelBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load PMX")) {
        world_->loadedModelPath = modelBuf;
    }

    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("##motion", motionBuf, sizeof(motionBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load VMD")) {
        world_->loadedMotionPath = motionBuf;
    }

    if (!world_->loadedModelPath.empty())
        ImGui::TextUnformatted(("Model: " + world_->loadedModelPath).c_str());
    if (!world_->loadedMotionPath.empty())
        ImGui::TextUnformatted(("Motion: " + world_->loadedMotionPath).c_str());

    ImGui::End();

    // -----------------------------------------------------------------------
    //  PMX Debug window  (always visible when a load was attempted)
    // -----------------------------------------------------------------------
    const auto& dbg = world_->loadDebug;
    if (dbg.attempted) {
        ImGui::SetNextWindowPos(ImVec2(10, 450), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_FirstUseEver);
        ImGui::Begin("PMX Debug");

        if (dbg.success) {
            ImGui::TextColored({0.2f,1.f,0.2f,1.f}, "STATUS: OK");
        } else {
            ImGui::TextColored({1.f,0.3f,0.3f,1.f}, "STATUS: FAILED");
            ImGui::TextWrapped("Failed at: %s",
                dbg.failedAt.empty() ? "(unknown)" : dbg.failedAt.c_str());
        }
        ImGui::Separator();
        ImGui::TextUnformatted(dbg.filePath.c_str());
        if (dbg.streamPosAtFail >= 0)
            ImGui::Text("Stream pos at fail: %" PRId64, dbg.streamPosAtFail);
        ImGui::Separator();

        auto statCell = [](const char* label, int v) {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            if (v < 0) ImGui::TextDisabled("—");
            else        ImGui::Text("%d", v);
        };
        if (ImGui::BeginTable("pmxstats", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Section"); ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Section"); ImGui::TableSetupColumn("Count");
            ImGui::TableHeadersRow();
            statCell("Vertices",  dbg.vertCount);
            statCell("Textures",  dbg.texCount);
            statCell("Indices",   dbg.idxCount);
            statCell("Bones",     dbg.boneCount);
            statCell("Materials", dbg.matCount);
            statCell("Morphs",    dbg.morphCount);
            ImGui::EndTable();
        }

        ImGui::End();
    }
}

} // namespace Phantom::Animation
