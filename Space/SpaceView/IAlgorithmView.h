#pragma once

#include "World.h"
#include <string>

namespace VKSpace {

class IAlgorithmView {
public:
    virtual ~IAlgorithmView() = default;
    virtual const char* getName() const = 0;
    virtual void onImGui(World& world) = 0;
    virtual void run(World& world) = 0;
    virtual bool setParam(const std::string& name, const std::string& value) = 0;
};

} // namespace VKSpace
