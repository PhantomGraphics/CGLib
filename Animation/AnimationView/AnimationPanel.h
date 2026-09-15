#pragma once

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "World.h"

namespace Phantom::Animation {

class AnimationViewApp;

class AnimationPanel : public ::VKG::IVkUIPanel {
public:
    void init(World* world) { world_ = world; }
    // Non-owning; only needed for the Environment section's IBL checkbox and "Load HDRI..."/
    // "Clear HDRI" buttons (see AnimationViewApp::loadEnvironmentHDR()'s comment).
    void setApp(AnimationViewApp* app) { app_ = app; }
    void onImGui() override;

private:
    World* world_ = nullptr;
    AnimationViewApp* app_ = nullptr;
};

} // namespace Phantom::Animation
