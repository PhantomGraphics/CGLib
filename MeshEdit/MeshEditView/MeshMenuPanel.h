#pragma once

#include "MeshWorld.h"

#include <functional>
#include <string>

class ViewShell;

namespace Phantom::MeshView {

// The "Control" window and the "Mesh" menu of MeshEditView. Nothing here edits the mesh directly: every button submits the
// same command string a scenario or the Command window would, so the GUI and the scripts cannot drift apart.
class MeshMenuPanel {
public:
    void init(MeshWorld* world) { world_ = world; }
    void setShell(ViewShell* shell) { shell_ = shell; }
    void setSubmit(std::function<void(const std::string&)> submit) { submit_ = std::move(submit); }
    void setLocked(bool locked) { locked_ = locked; }

    void onImGuiMenuBar();   // inside BeginMainMenuBar()
    void onImGui();          // the Control window (through the shell)

private:
    void send(const std::string& command) const { if (submit_) submit_(command); }

    MeshWorld* world_ = nullptr;
    ViewShell* shell_ = nullptr;
    std::function<void(const std::string&)> submit_;
    bool locked_ = false;

    // Operation parameters (the GUI's text boxes).
    int divs_ = 2;
    float distance_ = 0.3f, thickness_ = 0.08f, depth_ = 0.f, width_ = 0.1f, factor_ = 0.5f, angle_ = 180.f;
    float move_[3] = { 0.f, 0.2f, 0.f };
    int levels_ = 1;
    bool mirrorAxis_[3] = { true, false, false };
};

}  // namespace MeshView
