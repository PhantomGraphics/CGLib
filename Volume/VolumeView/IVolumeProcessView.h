#pragma once

#include "World.h"
#include <functional>

namespace VkVolumeView {

class IVolumeProcessView {
public:
    virtual ~IVolumeProcessView() = default;
    virtual const char* getName() const = 0;
    virtual void onImGui(World& world, int activeSceneId,
                         const std::function<void()>& onRebuild) = 0;
};

} // namespace VkVolumeView
