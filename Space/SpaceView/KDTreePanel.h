#pragma once

#include "IAlgorithmView.h"
#include "World.h"

namespace VKSpace {

/// @brief ImGui panel for KDTree algorithm.
class KDTreePanel : public IAlgorithmView {
public:
    const char* getName() const override { return "KDTree"; }
    void onImGui(World& world) override;
    void run(World& world) override;
    bool setParam(const std::string& name, const std::string& value) override;

private:
    int k_         = 8;
    int lineCount_ = 0;
};

} // namespace VKSpace
