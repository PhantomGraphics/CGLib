#pragma once

#include "IAlgorithmView.h"
#include "World.h"

namespace VKSpace {

/// @brief ImGui panel for Octree algorithm.
class OctreePanel : public IAlgorithmView {
public:
    const char* getName() const override { return "Octree"; }
    void onImGui(World& world) override;
    void run(World& world) override;
    bool setParam(const std::string& name, const std::string& value) override;

private:
    int maxDepth_  = 1;
    int nodeCount_ = 0;
};

} // namespace VKSpace
