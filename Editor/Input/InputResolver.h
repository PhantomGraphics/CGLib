#pragma once

#include "Bindings.h"

#include <functional>
#include <map>

namespace Phantom::Editor {

// Returns owned command strings: executing a command may change the keymap.
std::vector<std::string> triggerCommands(const BindingTable& table, const std::string& context,
                                        const InputSpec& event, bool release);
std::vector<std::string> deltaCommands(const BindingTable& table, const std::string& context,
                                      const InputSpec& event, double dx, double dy, double value);

// The host supplies device values and handles UI capture and modifier checks.
// Retains Universe's action policy: combine active-context and global bindings,
// invert axes, scale, clamp to [0,1], and take the maximum per named action.
using InputValueFn = std::function<float(const InputSpec&)>;
std::map<std::string, float> resolveActions(const BindingTable& table, const std::string& context,
                                          const InputValueFn& inputValue);

} // namespace Phantom::Editor
