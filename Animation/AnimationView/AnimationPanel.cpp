#include "AnimationPanel.h"
#include "AnimationViewApp.h"

#include "imgui.h"
#include "CGLib/VkAppBase/ScenarioRunner/ViewShell.h"
#include "../../ThirdParty/tinyfiledialogs/tinyfiledialogs.h"
#include <cinttypes>
#include <cstdio>

namespace Phantom::Animation {

namespace {
std::string fmtArg(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    return buf;
}
} // namespace

void AnimationPanel::onImGui()
{
    if (!world_) return;
    drawMain();
    drawDebug();
}

void AnimationPanel::drawMain()
{
    if (!shell_ || !shell_->beginPanel("Animation")) return;
    ImGui::BeginDisabled(locked_);

    const float duration = world_->duration;

    if (ImGui::Button(world_->playing ? "Stop" : "Play"))
        send(world_->playing ? "Stop" : "Play");
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        send("Reset");
    ImGui::SameLine();
    bool loop = world_->loop;
    if (ImGui::Checkbox("Loop", &loop))
        send(loop ? "SetLoop:1" : "SetLoop:0");

    char label[64];
    std::snprintf(label, sizeof(label), "%.2f / %.2f s", world_->currentTime, duration);
    float t = (duration > 0.f) ? world_->currentTime / duration : 0.f;
    if (ImGui::SliderFloat("Time", &t, 0.f, 1.f, label))
        send("SetTime:" + fmtArg(t * duration));

    float speed = world_->speed;
    if (ImGui::SliderFloat("Speed", &speed, 0.1f, 3.0f, "%.1fx"))
        send("SetSpeed:" + fmtArg(speed));

    ImGui::Separator();
    bool showMesh = world_->showMesh;
    if (ImGui::Checkbox("Show Mesh", &showMesh))
        send(showMesh ? "SetVisible:Mesh:1" : "SetVisible:Mesh:0");

    if (app_ && ImGui::CollapsingHeader("Environment")) {
        bool useIBL = app_->getUseIBL();
        if (ImGui::Checkbox("Use IBL", &useIBL)) send(useIBL ? "SetUseIBL:1" : "SetUseIBL:0");
        ImGui::Text("%s", app_->hasEnvironmentHDR() ? "Env: real HDRI" : "Env: placeholder");
        if (ImGui::Button("Load HDRI...")) {
            const char* filters[] = { "*.hdr" };
            const char* path = tinyfd_openFileDialog(
                "Load Environment HDRI", "", 1, filters, "Radiance HDR files (*.hdr)", 0);
            if (path) send(std::string("LoadEnvironmentHDR:") + path);
        }
        if (app_->hasEnvironmentHDR()) {
            ImGui::SameLine();
            if (ImGui::Button("Clear HDRI")) send("ClearEnvironmentHDR");
        }
    }

    ImGui::Separator();
    ImGui::Text("Bones: %d  Verts: %d  IK: %d  Morphs: %d",
        world_->boneCount, world_->vertCount, world_->ikCount, world_->morphCount);

    // IK -- informational only post-migration (see AnimationWorld.h: baked at load time).
    if (world_->ikCount > 0) {
        ImGui::Separator();
        bool ik = world_->ikEnabled;
        if (ImGui::Checkbox("IK Enabled", &ik)) send(ik ? "SetIKEnabled:1" : "SetIKEnabled:0");
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
    if (ImGui::Button("Load PMX"))
        send(std::string("LoadPMX:") + modelBuf);

    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("##motion", motionBuf, sizeof(motionBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load VMD"))
        send(std::string("LoadVMD:") + motionBuf);

    if (!world_->loadedModelPath.empty())
        ImGui::TextUnformatted(("Model: " + world_->loadedModelPath).c_str());
    if (!world_->loadedMotionPath.empty())
        ImGui::TextUnformatted(("Motion: " + world_->loadedMotionPath).c_str());

    ImGui::EndDisabled();
    shell_->endPanel();
}

void AnimationPanel::drawDebug()
{
    // -----------------------------------------------------------------------
    //  PMX Debug window  (only offered once a load was attempted)
    // -----------------------------------------------------------------------
    const auto& dbg = world_->loadDebug;
    if (dbg.attempted && shell_ && shell_->beginPanel("PMX Debug")) {

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

        shell_->endPanel();
    }
}

} // namespace Phantom::Animation
