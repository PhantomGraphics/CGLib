#include "MCMeshView.h"

#include "World.h"
#include "VolumeScene.h"

#include "../Volume/MCSurfaceBuilder.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"

#include "imgui.h"

#include <string>

namespace VkVolumeView {

void MCMeshView::onImGui(World& world, int activeSceneId,
                          const std::function<void()>& onRebuild)
{
    ImGui::SliderFloat("Iso level", &isoLevel_, -5.0f, 5.0f);

    const bool hasScene = (world.findById(activeSceneId) != nullptr);
    ImGui::BeginDisabled(!hasScene);
    if (ImGui::Button("Run MC")) {
        auto* scene = world.findById(activeSceneId);
        if (scene && scene->getShape()) {
            if (scene->getShape()->getActiveVoxelCount() > 0) {
                Phantom::Volume::MCSurfaceBuilder builder;
                builder.build(*scene->getShape(), isoLevel_);
                const auto& tris = builder.getTriangles();

                PolygonMesh mesh;
                mesh.name = "MC_" + scene->getName();

                mesh.positions.reserve(tris.size() * 9);
                mesh.colors.reserve(tris.size() * 12);
                uint32_t idx = 0;
                for (const auto& tri : tris) {
                    const auto& verts = tri.getVertices();
                    for (int vi = 0; vi < 3; ++vi) {
                        mesh.positions.push_back(static_cast<float>(verts[vi].x));
                        mesh.positions.push_back(static_cast<float>(verts[vi].y));
                        mesh.positions.push_back(static_cast<float>(verts[vi].z));
                        mesh.colors.push_back(0.8f);
                        mesh.colors.push_back(0.8f);
                        mesh.colors.push_back(0.9f);
                        mesh.colors.push_back(0.85f);
                        mesh.indices.push_back(idx++);
                    }
                }

                world.clearPolygons();
                world.addPolygon(std::move(mesh));

                const int voxels = scene->getShape()->getActiveVoxelCount();
                statusMsg_ = "MC done: " + std::to_string(tris.size()) + " triangles ("
                           + std::to_string(voxels) + " active voxels)";
                if (onRebuild) onRebuild();
            } else {
                statusMsg_ = "Error: empty sparse volume";
            }
        }
    }
    ImGui::EndDisabled();

    if (!statusMsg_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", statusMsg_.c_str());
    }
}

} // namespace VkVolumeView
