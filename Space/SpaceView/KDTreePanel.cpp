#include "KDTreePanel.h"

#include "imgui.h"
#include "../Space/KDTree.h"
#include "../../../CGLib/Math/Vector3d.h"
#include "../../../CGLib/Math/Box3d.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <vector>

using Phantom::Math::Vector3df;
using Phantom::Space::KDTree;

namespace VKSpace {

void KDTreePanel::onImGui(World& world) {
    ImGui::SliderInt("k", &k_, 1, 20);
    if (ImGui::Button("Run")) run(world);
    if (lineCount_ > 0) {
        ImGui::Separator();
        ImGui::Text("Line indices: %d", lineCount_);
    }
}

void KDTreePanel::run(World& world) {
    const Phantom::Math::Box3df box(
        Vector3df(-1.f, -1.f, -1.f),
        Vector3df( 1.f,  1.f,  1.f));

    const int nx = 5, ny = 5, nz = 5;
    const auto mn  = box.getMin();
    const auto len = box.getLength();

    std::vector<Vector3df> storage;
    storage.reserve(nx * ny * nz);

    KDTree kd;
    for (int ix = 0; ix < nx; ++ix)
        for (int iy = 0; iy < ny; ++iy)
            for (int iz = 0; iz < nz; ++iz) {
                const float ux = (ix + 0.5f) / nx;
                const float uy = (iy + 0.5f) / ny;
                const float uz = (iz + 0.5f) / nz;
                storage.push_back(mn + Vector3df(len.x * ux, len.y * uy, len.z * uz));
            }
    kd.build(storage);

    auto& res = world.getResult();
    res.clear();

    const float sx   = len.x / nx;
    const float sy   = len.y / ny;
    const float sz   = len.z / nz;
    const float half = std::min({sx, sy, sz}) * 0.2f;

    const int kCount = std::max(1, k_);
    for (const auto& p : storage) {
        res.addBoxWireframe(
            glm::vec3(p.x - half, p.y - half, p.z - half),
            glm::vec3(p.x + half, p.y + half, p.z + half),
            {0.f, 1.f, 0.f, 1.f});

        std::vector<std::pair<float, Vector3df>> nearest;
        nearest.reserve(storage.size());
        for (const auto& qp : storage) {
            if (&qp == &p) continue;
            const float dx = qp.x - p.x;
            const float dy = qp.y - p.y;
            const float dz = qp.z - p.z;
            const float dist2 = dx * dx + dy * dy + dz * dz;
            nearest.emplace_back(dist2, qp);
        }
        std::sort(nearest.begin(), nearest.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        const int linkCount = std::min(kCount, static_cast<int>(nearest.size()));
        for (int i = 0; i < linkCount; ++i) {
            const auto& np = nearest[i].second;
            res.addLine(glm::vec3(p.x, p.y, p.z), glm::vec3(np.x, np.y, np.z),
                        glm::vec4(0.2f, 1.0f, 0.2f, 0.45f));
        }
    }

    lineCount_ = static_cast<int>(res.lineIndices.size());
    world.markDirty();
}

bool KDTreePanel::setParam(const std::string& name, const std::string& value) {
    if (name == "k") {
        int v;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec == std::errc{}) { k_ = v; return true; }
        return false;
    }
    return false;
}

} // namespace VKSpace
