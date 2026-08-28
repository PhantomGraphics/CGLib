#pragma once

#include "IAlgorithmView.h"
#include "World.h"

namespace VKSpace {

/// @brief ImGui panel for CompactSpaceHash algorithm.
class CompactSpaceHashPanel : public IAlgorithmView {
public:
    const char* getName() const override { return "CompactSpaceHash"; }
    void onImGui(World& world) override;
    void run(World& world) override;
    bool setParam(const std::string& name, const std::string& value) override;

private:
    float  searchRadius_   = 0.35f;
    int    totalNeighbors_ = 0;
};

} // namespace VKSpace
