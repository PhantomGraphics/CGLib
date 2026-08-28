#pragma once
#include <string>
#include <vector>

// Abstract interface for scenario runner command dispatchers.
// Implement this in each app's command dispatcher to enable ScenarioRunner support.
class IScenarioDispatcher {
public:
    virtual void dispatch(const std::string& command) = 0;
    virtual std::vector<std::string> collectResponses() = 0;
    virtual ~IScenarioDispatcher() = default;
};
