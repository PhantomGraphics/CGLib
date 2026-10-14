#include "MeshMenuPanel.h"

#include "imgui.h"

#include "CGLib/VkAppBase/ScenarioRunner/ViewShell.h"

#include <cstdio>

using namespace Phantom::MeshEdit;

namespace Phantom::MeshView {

namespace {

std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

}  // namespace

void MeshMenuPanel::onImGuiMenuBar() {
    if (!ImGui::BeginMenu("Mesh")) return;
    ImGui::BeginDisabled(locked_);
    if (ImGui::MenuItem("New Box")) send("NewMesh:box");
    if (ImGui::MenuItem("New Plane")) send("NewMesh:plane divs=" + std::to_string(divs_));
    ImGui::Separator();
    if (ImGui::MenuItem("Edit Mode", nullptr, world_ && world_->editMode())) send(world_ && world_->editMode() ? "SetMode:object" : "SetMode:edit");
    if (ImGui::MenuItem("Undo", nullptr, false, world_ && world_->undoDepth() > 0)) send("Undo");
    if (ImGui::MenuItem("Redo", nullptr, false, world_ && world_->redoDepth() > 0)) send("Redo");
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Camera")) send("ResetCamera");
    ImGui::EndDisabled();
    if (shell_ && ImGui::MenuItem("Show Control Window")) shell_->setPanelVisible("Control", true);
    ImGui::EndMenu();
}

void MeshMenuPanel::onImGui() {
    if (!shell_ || !shell_->beginPanel("Control")) return;
    if (!world_) { shell_->endPanel(); return; }
    ImGui::BeginDisabled(locked_);

    ImGui::TextUnformatted(world_->describe().c_str());
    ImGui::Separator();

    // ---- mesh ----
    if (ImGui::Button("Box")) send("NewMesh:box");
    ImGui::SameLine();
    if (ImGui::Button("Plane")) send("NewMesh:plane divs=" + std::to_string(divs_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::SliderInt("divs", &divs_, 1, 8);

    // ---- mode and selection ----
    bool edit = world_->editMode();
    if (ImGui::Checkbox("Edit mode", &edit)) send(edit ? "SetMode:edit" : "SetMode:object");
    ImGui::SameLine();
    bool xray = world_->edit().xray;
    if (ImGui::Checkbox("X-ray pick", &xray)) send(std::string("SetXray:") + (xray ? "1" : "0"));

    ImGui::BeginDisabled(!edit);
    const ElementKind kind = world_->edit().kind;
    if (ImGui::RadioButton("Vertex", kind == ElementKind::Vertex)) send("SelectElement:vertex");
    ImGui::SameLine();
    if (ImGui::RadioButton("Edge", kind == ElementKind::Edge)) send("SelectElement:edge");
    ImGui::SameLine();
    if (ImGui::RadioButton("Face", kind == ElementKind::Face)) send("SelectElement:face");
    if (ImGui::Button("Select all")) send("Select:all");
    ImGui::SameLine();
    if (ImGui::Button("Clear")) send("Select:none");
    ImGui::SameLine();
    ImGui::Text("%zu selected", world_->edit().ids.size());
    ImGui::TextDisabled("Click / Shift+click / Ctrl+click in the view");

    // ---- operations ----
    ImGui::Separator();
    ImGui::SetNextItemWidth(110); ImGui::DragFloat("##distance", &distance_, 0.01f, -2.f, 2.f, "distance %.2f");
    ImGui::SameLine();
    if (ImGui::Button("Extrude")) send("Extrude:distance=" + num(distance_));
    ImGui::SetNextItemWidth(110); ImGui::DragFloat("##thickness", &thickness_, 0.005f, 0.001f, 1.f, "thickness %.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80); ImGui::DragFloat("##depth", &depth_, 0.01f, -1.f, 1.f, "depth %.2f");
    ImGui::SameLine();
    if (ImGui::Button("Inset")) send("Inset:thickness=" + num(thickness_) + " depth=" + num(depth_));
    ImGui::SetNextItemWidth(110); ImGui::DragFloat("##width", &width_, 0.005f, 0.001f, 1.f, "width %.3f");
    ImGui::SameLine();
    if (ImGui::Button("Bevel")) send("Bevel:width=" + num(width_));
    ImGui::SetNextItemWidth(110); ImGui::DragFloat("##factor", &factor_, 0.01f, 0.01f, 0.99f, "factor %.2f");
    ImGui::SameLine();
    if (ImGui::Button("Loop cut")) send("LoopCut:factor=" + num(factor_));
    ImGui::SetNextItemWidth(110); ImGui::DragFloat("##angle", &angle_, 1.f, 0.f, 180.f, "angle %.0f");
    ImGui::SameLine();
    if (ImGui::Button("Smooth")) send("Shade:smooth angle=" + num(angle_));
    ImGui::SameLine();
    if (ImGui::Button("Flat")) send("Shade:flat");
    if (ImGui::Button("Merge")) send("Merge:center");
    ImGui::SameLine();
    if (ImGui::Button("Delete")) send("Delete");
    ImGui::SetNextItemWidth(200); ImGui::DragFloat3("##move", move_, 0.01f, -2.f, 2.f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Move")) send("Move:" + num(move_[0]) + "," + num(move_[1]) + "," + num(move_[2]));
    ImGui::EndDisabled();   // !edit

    // ---- modifiers ----
    ImGui::Separator();
    ImGui::Text("Modifiers (%zu)", world_->modifiers().size());
    ImGui::Checkbox("x", &mirrorAxis_[0]); ImGui::SameLine();
    ImGui::Checkbox("y", &mirrorAxis_[1]); ImGui::SameLine();
    ImGui::Checkbox("z", &mirrorAxis_[2]); ImGui::SameLine();
    if (ImGui::Button("Mirror")) {
        std::string axes;
        for (int i = 0; i < 3; ++i) if (mirrorAxis_[i]) axes += "xyz"[i];
        send("AddModifier:mirror axis=" + (axes.empty() ? std::string("x") : axes));
    }
    ImGui::SetNextItemWidth(70); ImGui::SliderInt("levels", &levels_, 1, 4);
    ImGui::SameLine();
    if (ImGui::Button("Subdivision")) send("AddModifier:subdivision levels=" + std::to_string(levels_));
    if (ImGui::Button("Clear modifiers")) send("ClearModifiers");
    ImGui::SameLine();
    if (ImGui::Button("Apply modifiers")) send("ApplyModifiers");

    // ---- history ----
    ImGui::Separator();
    ImGui::BeginDisabled(world_->undoDepth() == 0);
    if (ImGui::Button("Undo")) send("Undo");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(world_->redoDepth() == 0);
    if (ImGui::Button("Redo")) send("Redo");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset camera")) send("ResetCamera");

    // ---- display ----
    ImGui::Separator();
    Display d = world_->display();
    bool changed = false;
    changed |= ImGui::Checkbox("Faces", &d.faces); ImGui::SameLine();
    changed |= ImGui::Checkbox("Edges", &d.edges); ImGui::SameLine();
    changed |= ImGui::Checkbox("Vertices", &d.vertices);
    changed |= ImGui::Checkbox("Normals", &d.normals); ImGui::SameLine();
    changed |= ImGui::Checkbox("Face orientation (blue = front, red = back)", &d.orientation);
    if (changed)
        send(std::string("SetDisplay:faces=") + (d.faces ? "1" : "0") + " edges=" + (d.edges ? "1" : "0") + " vertices=" +
             (d.vertices ? "1" : "0") + " normals=" + (d.normals ? "1" : "0") + " orientation=" + (d.orientation ? "1" : "0"));

    ImGui::EndDisabled();   // locked_
    shell_->endPanel();
}

}  // namespace MeshView
