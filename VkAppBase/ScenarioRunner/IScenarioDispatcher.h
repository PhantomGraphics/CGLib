#pragma once
#include "CommandInfo.h"
#include <string>
#include <vector>

// Abstract interface for scenario runner command dispatchers.
// Implement this in each app's command dispatcher to enable ScenarioRunner support.
class IScenarioDispatcher {
public:
    virtual void dispatch(const std::string& command) = 0;
    virtual std::vector<std::string> collectResponses() = 0;

    // Commands this dispatcher routes, for the command window's help and
    // completion. Optional: an empty catalog only disables those two features.
    virtual std::vector<CommandInfo> commandCatalog() const { return {}; }

    virtual ~IScenarioDispatcher() = default;
};
