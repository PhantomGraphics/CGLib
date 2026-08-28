#include "OctreePanel.h"

#include "imgui.h"
#include "../Space/Octree.h"
#include "../../../CGLib/Math/Box3d.h"
#include "../../../CGLib/Math/Vector3d.h"

#include <charconv>
#include <functional>

using Phantom::Math::Vector3df;
using Phantom::Math::Box3df;
using Phantom::Space::Octree;

namespace VKSpace {

void OctreePanel::onImGui(World& world) {
    ImGui::SliderInt("Max Depth", &maxDepth_, 1, 8);
    if (ImGui::Button("Run")) run(world);
    if (nodeCount_ > 0) {
        ImGui::Separator();
        ImGui::Text("Node count: %d", nodeCount_);
    }
}

void OctreePanel::run(World& world) {
    const Box3df box(Vector3df(-1.f, -1.f, -1.f), Vector3df(1.f, 1.f, 1.f));
    Octree tree(box);

    const int depth = std::max(1, maxDepth_);
    std::function<void(Octree&, int)> build = [&](Octree& node, int remain) {
        if (remain <= 0) return;
        node.createChildren();
        for (auto& child : node.getChildren())
            if (child) build(*child, remain - 1);
    };
    build(tree, depth);

    auto& res = world.getResult();
    res.clear();
    nodeCount_ = 0;

    std::function<void(const Octree&, int)> collect = [&](const Octree& node, int level) {
        if (level > depth) return;
        const auto b  = node.getBox();
        const auto mn = b.getMin();
        const auto mx = b.getMax();
        res.addBoxWireframe(
            glm::vec3(mn.x, mn.y, mn.z),
            glm::vec3(mx.x, mx.y, mx.z),
            {1.f, 0.5f, 0.f, 1.f});
        ++nodeCount_;
        for (const auto& child : node.getChildren())
            if (child) collect(*child, level + 1);
    };
    collect(tree, 1);

    world.markDirty();
}

bool OctreePanel::setParam(const std::string& name, const std::string& value) {
    if (name == "MaxDepth") {
        int v;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec == std::errc{}) { maxDepth_ = v; return true; }
        return false;
    }
    return false;
}

} // namespace VKSpace
