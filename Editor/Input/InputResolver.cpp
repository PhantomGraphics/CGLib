#include "InputResolver.h"

#include <algorithm>

namespace Phantom::Editor {

std::vector<std::string> triggerCommands(const BindingTable& table, const std::string& context,
                                        const InputSpec& event, bool release) {
    std::vector<std::string> commands;
    for (const Binding* binding : table.resolve(context, event))
        if (binding->type == BindingType::Trigger && binding->onRelease == release)
            commands.push_back(binding->command);
    return commands;
}

std::vector<std::string> deltaCommands(const BindingTable& table, const std::string& context,
                                      const InputSpec& event, double dx, double dy, double value) {
    std::vector<std::string> commands;
    for (const Binding* binding : table.resolve(context, event))
        if (binding->type == BindingType::Delta)
            commands.push_back(expandCommand(*binding, dx, dy, value));
    return commands;
}

std::map<std::string, float> resolveActions(const BindingTable& table, const std::string& context,
                                          const InputValueFn& inputValue) {
    std::map<std::string, float> values;
    if (!inputValue) return values;
    for (const Binding& binding : table.all()) {
        if (binding.type != BindingType::Action) continue;
        if (binding.context != context && binding.context != "global") continue;
        float raw = inputValue(binding.input);
        if (binding.input.kind == InputKind::Axis && binding.invert) raw = -raw;
        const float value = std::clamp(std::max(0.f, raw) * static_cast<float>(binding.scale), 0.f, 1.f);
        float& slot = values[binding.action];
        slot = std::max(slot, value);
    }
    return values;
}

} // namespace Phantom::Editor
