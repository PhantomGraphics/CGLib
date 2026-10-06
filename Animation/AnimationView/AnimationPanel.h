#pragma once

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "World.h"

#include <functional>
#include <string>

class ViewShell;

namespace Phantom::Animation {

class AnimationViewApp;

class AnimationPanel : public ::VKG::IVkUIPanel {
public:
    void init(World* world) { world_ = world; }
    // Non-owning; only needed for the Environment section's IBL checkbox and "Load HDRI..."/
    // "Clear HDRI" buttons (see AnimationViewApp::loadEnvironmentHDR()'s comment).
    void setApp(AnimationViewApp* app) { app_ = app; }
    void onImGui() override;

    // The windows are shell panels (hidden until opened). Every GUI action is sent as a
    // command through `submit`, i.e. the path a typed or scenario command takes.
    void setShell(ViewShell* s) { shell_ = s; }
    void setSubmit(std::function<void(const std::string&)> f) { submit_ = std::move(f); }
    void setLocked(bool v) { locked_ = v; }   // scenario running: controls shown but disabled

private:
    void drawMain();
    void drawDebug();
    void send(const std::string& cmd) { if (submit_) submit_(cmd); }

    ViewShell* shell_ = nullptr;
    std::function<void(const std::string&)> submit_;
    bool locked_ = false;
    World* world_ = nullptr;
    AnimationViewApp* app_ = nullptr;
};

} // namespace Phantom::Animation
