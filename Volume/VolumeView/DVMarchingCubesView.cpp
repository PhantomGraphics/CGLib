#include "DVMarchingCubesView.h"

#include "DenseVolumeScene.h"

#include "../Volume/MCSurfaceBuilder.h"

#include "imgui.h"

#include <cstdint>
#include <string>

namespace VkVolumeView {

void DVMarchingCubesView::onImGui(World& world, int activeSceneId,
								  const std::function<void()>& onRebuild)
{
	ImGui::SliderFloat("Iso level", &isoLevel_, -5.0f, 5.0f);

	auto* scene = world.findDenseById(activeSceneId);
	const bool canRun = (scene && scene->getVolume());

	ImGui::BeginDisabled(!canRun);
	if (ImGui::Button("Run Dense MC")) {
		Phantom::Volume::MCSurfaceBuilder builder;
		builder.build(*scene->getVolume(), isoLevel_);
		const auto& tris = builder.getTriangles();

		PolygonMesh mesh;
		mesh.name = "DMC_" + scene->getName();
		mesh.positions.reserve(tris.size() * 9);
		mesh.colors.reserve(tris.size() * 12);

		uint32_t idx = 0;
		for (const auto& tri : tris) {
			const auto& verts = tri.getVertices();
			for (int vi = 0; vi < 3; ++vi) {
				mesh.positions.push_back(static_cast<float>(verts[vi].x));
				mesh.positions.push_back(static_cast<float>(verts[vi].y));
				mesh.positions.push_back(static_cast<float>(verts[vi].z));
				mesh.colors.push_back(0.7f);
				mesh.colors.push_back(0.9f);
				mesh.colors.push_back(0.8f);
				mesh.colors.push_back(0.85f);
				mesh.indices.push_back(idx++);
			}
		}

		world.clearPolygons();
		world.addPolygon(std::move(mesh));

		statusMsg_ = "Dense MC done: " + std::to_string(tris.size()) + " triangles";
		if (onRebuild) onRebuild();
	}
	ImGui::EndDisabled();

	if (!statusMsg_.empty()) {
		ImGui::Spacing();
		ImGui::TextWrapped("%s", statusMsg_.c_str());
	}
}

} // namespace VkVolumeView
