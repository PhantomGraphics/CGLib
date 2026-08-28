#include "SVSphereView.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Coord.h"

#include "imgui.h"

#define GLM_FORCE_RADIANS
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace VkVolumeView {

void SVSphereView::onImGui(World& world, int /*activeSceneId*/,
                            const std::function<void()>& onRebuild)
{
    ImGui::InputFloat3("Center", center_);
    ImGui::SliderFloat("Radius", &radius_, 1.0f, 100.f);
    ImGui::SliderFloat("Cell length", &cell_, 0.1f, 5.0f);

    if (ImGui::Button("Create")) {
        const float cell = std::max(cell_, 0.05f);
        const float r    = std::max(radius_, 0.01f);
        // Band thickness: 3 voxels each side of the surface.
        const float band = cell * 3.0f;

        auto volume = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
        volume->setVoxelSize(cell);

        // World-space AABB of the narrow band around the sphere surface.
        const glm::vec3 cx(center_[0], center_[1], center_[2]);

        const float lo = -(r + band);
        const float hi =  (r + band);

        const int iLo = static_cast<int>(std::floor((cx.x + lo) / cell));
        const int iHi = static_cast<int>(std::ceil ((cx.x + hi) / cell));
        const int jLo = static_cast<int>(std::floor((cx.y + lo) / cell));
        const int jHi = static_cast<int>(std::ceil ((cx.y + hi) / cell));
        const int kLo = static_cast<int>(std::floor((cx.z + lo) / cell));
        const int kHi = static_cast<int>(std::ceil ((cx.z + hi) / cell));

        for (int i = iLo; i <= iHi; ++i) {
            for (int j = jLo; j <= jHi; ++j) {
                for (int k = kLo; k <= kHi; ++k) {
                    const glm::vec3 wp(i * cell, j * cell, k * cell);
                    const float sdf = glm::distance(wp, cx) - r;
                    if (std::abs(sdf) <= band) {
                        volume->setValue(Phantom::Volume::Coord(i, j, k), sdf);
                    }
                }
            }
        }

        const std::string name = "Sphere_" + std::to_string(count_++);
        auto* scene = world.addScene(name);
        scene->setShape(std::move(volume));
        statusMsg_ = "Created: " + name +
                     " (" + std::to_string(scene->getShape()->getActiveVoxelCount()) + " vx)";
        if (onRebuild) onRebuild();
    }

    if (!statusMsg_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", statusMsg_.c_str());
    }
}

} // namespace VkVolumeView
