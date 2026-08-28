#pragma once

#include "IVolumeProcessView.h"

namespace VkVolumeView {

/**
 * @brief Placeholder IVolumeProcessView that explains the new SPH-to-volume
 *        workflow (use VkFluidView -> "Volume > SPH to Volume").
 *
 * The actual SPH conversion logic (SPHVolumeConverter) lives in
 * Physics/VkFluidView/VkSPHVolumePanel so that VkVolumeView does not
 * depend on the Physics/Fluid library.
 */
class SVParticleView : public IVolumeProcessView {
public:
    const char* getName() const override { return "Particles to Volume (SPH)"; }

    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;
};

} // namespace VkVolumeView
