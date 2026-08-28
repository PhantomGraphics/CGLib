#pragma once

#include "IAlgorithmView.h"
#include "World.h"

namespace VKSpace {

/// @brief ImGui panel for SignedDistance algorithm.
class SignedDistancePanel : public IAlgorithmView {
public:
    const char* getName() const override { return "SignedDistance"; }
    void onImGui(World& world) override;
    void run(World& world) override;
    bool setParam(const std::string& name, const std::string& value) override;

private:
    float  sphereRadius_ = 0.5f;
    int    samples_      = 10;
    int    sampleCount_  = 0;
    float  minSdf_       = 0.f;
    float  maxSdf_       = 0.f;
};

} // namespace VKSpace
