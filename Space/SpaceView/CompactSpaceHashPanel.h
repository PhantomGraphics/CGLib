#pragma once

#include "IAlgorithmView.h"
#include "SpaceDemos.h"
#include "World.h"

namespace VKSpace {

/// @brief ImGui panel for the CompactSpaceHash demo. The computation lives in SpaceDemos.cpp; this class only edits
/// the parameters, requests a run (button or command) and shows the statistics of the last run.
class CompactSpaceHashPanel : public IAlgorithmView {
public:
    const char* getName() const override { return "CompactSpaceHash"; }
    void onImGui(World& world) override;
    void run(World& world) override;
    bool setParam(const std::string& name, const std::string& value) override;

private:
    HashDemoParams params_;
    HashDemoStats  stats_;
};

} // namespace VKSpace
