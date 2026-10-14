#pragma once

#include <map>
#include <string>

namespace Phantom::Editor {

// UI/device-independent named action values. The host defines the vocabulary and
// writes resolved values each frame. Scenario overrides survive those updates.
class ActionState {
public:
    void setValue(const std::string& action, float value) { values_[action] = value; }
    float value(const std::string& action) const {
        const auto injected = injected_.find(action);
        if (injected != injected_.end()) return injected->second ? 1.f : 0.f;
        const auto it = values_.find(action);
        return it == values_.end() ? 0.f : it->second;
    }
    bool isPressed(const std::string& action) const { return value(action) > 0.5f; }
    void inject(const std::string& action, bool pressed) { injected_[action] = pressed; }
    void clearInjected() { injected_.clear(); }

private:
    std::map<std::string, float> values_;
    std::map<std::string, bool> injected_;
};

} // namespace Phantom::Editor
