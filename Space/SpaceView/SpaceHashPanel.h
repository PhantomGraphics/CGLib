#pragma once

#include "IAlgorithmView.h"
#include "SpaceDemos.h"
#include "World.h"

namespace Phantom::VKSpace {

/// @brief ImGui panel for the SpaceHash demo. The computation lives in SpaceDemos.cpp; this class only edits
/// the parameters, requests a run (button or command) and shows the statistics of the last run.
class SpaceHashPanel : public IAlgorithmView {
public:
    const char* getName() const override { return "SpaceHash"; }
    void onImGui(World& world) override;
    void run(World& world) override;
    bool setParam(const std::string& name, const std::string& value) override;

private:
    HashDemoParams params_;
    HashDemoStats  stats_;
};

} // namespace VKSpace
