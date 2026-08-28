#pragma once

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "World.h"

namespace Phantom::Animation {

class AnimationPanel : public ::VKG::IVkUIPanel {
public:
    void init(World* world) { world_ = world; }
    void onImGui() override;

private:
    World* world_ = nullptr;
};

} // namespace Phantom::Animation
