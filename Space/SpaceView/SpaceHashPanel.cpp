#include "SpaceHashPanel.h"

#include "imgui.h"
#include "../Space/SpaceHash.h"
#include "../../../CGLib/Math/Vector3d.h"
#include "../../../CGLib/Math/Box3d.h"

#include <algorithm>
#include <charconv>

using Phantom::Math::Vector3df;
using Phantom::Space::SpaceHash;

namespace VKSpace {

void SpaceHashPanel::onImGui(World& world) {
    ImGui::SliderFloat("Search Radius", &searchRadius_, 0.05f, 1.0f);
    if (ImGui::Button("Run")) run(world);
    if (totalNeighbors_ > 0) {
        ImGui::Separator();
        ImGui::Text("Neighbor references: %d", totalNeighbors_);
    }
}

void SpaceHashPanel::run(World& world) {
    const Phantom::Math::Box3df box(
        Vector3df(-1.f, -1.f, -1.f),
        Vector3df( 1.f,  1.f,  1.f));
    const float radius = std::max(searchRadius_, 0.01f);

    const int nx = 6, ny = 6, nz = 6;
    std::vector<Vector3df> points;
    points.reserve(nx * ny * nz);

    const auto mn  = box.getMin();
    const auto len = box.getLength();
    for (int ix = 0; ix < nx; ++ix)
        for (int iy = 0; iy < ny; ++iy)
            for (int iz = 0; iz < nz; ++iz) {
                const float ux = (ix + 0.5f) / nx;
                const float uy = (iy + 0.5f) / ny;
                const float uz = (iz + 0.5f) / nz;
                points.emplace_back(mn + Vector3df(len.x * ux, len.y * uy, len.z * uz));
            }

    SpaceHash hash(radius, static_cast<int>(points.size() * 2 + 1));
    for (const auto& p : points)
        hash.add(p);

    auto& res = world.getResult();
    res.clear();

    const float half = std::min(radius * 0.2f, 0.08f);
    int totalNeighbors = 0;
    for (const auto& p : points) {
        const auto neighbors = hash.findNeighborIndices(p);
        totalNeighbors += static_cast<int>(neighbors.size());
        if (neighbors.size() > 1) {
            res.addBoxWireframe(
                glm::vec3(p.x - half, p.y - half, p.z - half),
                glm::vec3(p.x + half, p.y + half, p.z + half),
                {1.f, 1.f, 0.f, 1.f});
        }
    }
    totalNeighbors_ = totalNeighbors;
    world.markDirty();
}

bool SpaceHashPanel::setParam(const std::string& name, const std::string& value) {
    if (name == "SearchRadius") {
        float v;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec == std::errc{}) { searchRadius_ = v; return true; }
        return false;
    }
    return false;
}

} // namespace VKSpace
