#pragma once
#include "IScenarioHost.h"
#include "../IVkSubRenderer.h"
#include <string>
#include <vector>

// ImGui panel that lists *.json scenario files in a folder and lets the
// user run one, a checked subset, or all of them consecutively without
// closing the app. Driven purely through IScenarioHost so it has no
// dependency on any concrete app type.
class ScenarioBrowserPanel : public ::VKG::IVkUIPanel {
public:
    void setHost(IScenarioHost* host) { host_ = host; }
    void setDefaultFolder(const std::string& folder);
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    void onImGui() override;

private:
    struct Entry {
        std::string path;   // path passed to loadScenario()
        std::string name;   // file name shown in the table
        bool        checked = false;
        enum class Status { Pending, Running, Passed, Failed } status = Status::Pending;
        size_t      steps = 0;
        std::string failMessage;
    };

    void refresh();
    void tickQueue();
    void drawUI();

    IScenarioHost* host_ = nullptr;
    bool           initialized_ = false;
    bool           visible_ = true;

    char folderBuf_[260] = "scenarios";
    std::vector<Entry> entries_;

    std::vector<size_t> queue_;
    int                  currentIndex_ = -1; // index into entries_, -1 = idle
};
