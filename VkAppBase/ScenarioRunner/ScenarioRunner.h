#pragma once
#include "IScenarioDispatcher.h"
#include <string>
#include <unordered_map>
#include <vector>

class ScenarioRunner {
public:
    // Load a scenario JSON file. Returns false on parse error.
    bool load(const std::string& jsonPath);

    // Call once per frame from onUpdate().
    // Feeds responses from the previous frame and issues the next command.
    // Returns true when the scenario has finished (pass or fail).
    bool tick(IScenarioDispatcher& dispatcher,
              const std::vector<std::string>& responses);

    bool isActive()   const { return !steps_.empty() && !finished_; }
    bool isFinished() const { return finished_; }
    bool hasFailed()  const { return failed_;   }
    const std::string& failMessage() const { return failMsg_; }
    size_t stepCount() const { return steps_.size(); }

private:
    struct Step {
        std::string label;
        std::string command;
        std::string expect;
        std::string expectNot;
        std::string expectPrefix;
        std::string expectNotPrefix;
        std::string expectRange;
        std::string storeAs;
        std::string stripPrefix;
    };

    std::string expandVars(const std::string& s) const;
    bool        checkResponse(const Step& step, const std::string& resp);
    bool        evaluatePostAssert();

    std::vector<Step>                            steps_;
    std::string                                  postAssert_;
    size_t                                       current_  = 0;
    bool                                         waiting_  = false;
    bool                                         finished_ = false;
    bool                                         failed_   = false;
    std::string                                  failMsg_;
    std::unordered_map<std::string, std::string> vars_;
};
