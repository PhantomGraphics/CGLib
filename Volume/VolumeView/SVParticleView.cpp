// SVParticleView.cpp
// SPH particle -> SparseVolume conversion has been moved to VkFluidView
// (Physics/VkFluidView/VkSPHVolumePanel). Use the "Volume > SPH to Volume"
// menu in VkFluidView to build volumes from simulation particles and export
// them as .vdb files, then import the result here via "Volume > Import VDB".
//
// This translation unit is intentionally empty so that VkVolumeView no longer
// depends on the Physics/Fluid library.

#include "SVParticleView.h"

#include "World.h"

#include "imgui.h"

namespace VkVolumeView {

void SVParticleView::onImGui(World& /*world*/, int /*activeSceneId*/,
                              const std::function<void()>& /*onRebuild*/)
{
    ImGui::TextWrapped(
        "SPH particle-to-volume conversion has been moved to VkFluidView.\n\n"
        "Workflow:\n"
        "  1. Run a simulation in VkFluidView.\n"
        "  2. Open \"Volume > SPH to Volume\" in VkFluidView.\n"
        "  3. Build the volume and save it as a .vdb file.\n"
        "  4. Import the .vdb here via \"Volume > Import VDB\".");
}

} // namespace VkVolumeView
