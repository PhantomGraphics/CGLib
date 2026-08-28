#pragma once
#include <string>

// Abstract interface implemented by each Xxx App class so that
// ScenarioBrowserPanel can drive scenario runs without depending on
// any concrete app type. All methods forward to the app's own
// ScenarioRunner member.
class IScenarioHost {
public:
    virtual bool   loadScenario(const std::string& jsonPath) = 0;
    virtual void   setExitOnScenarioComplete(bool v) = 0;
    virtual bool   isScenarioActive() const = 0;
    virtual bool   scenarioHasFailed() const = 0;
    virtual const std::string& scenarioFailMessage() const = 0;
    virtual size_t scenarioStepCount() const = 0;
    virtual ~IScenarioHost() = default;
};
