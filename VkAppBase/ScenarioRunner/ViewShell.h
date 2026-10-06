#pragma once
// Standard screen of a test viewer: render area + main menu + Command window +
// Outliner. Every other panel is hidden until opened from the View menu or by
// selecting an outliner entry. Visibility is saved in imgui.ini next to the
// window positions (own "[ViewShell][Panels]" section).
//
// Command input goes through the app's IScenarioDispatcher, i.e. the same
// path ScenarioRunner uses, so a scenario "command" string can be typed as-is.
#include "CommandConsoleModel.h"
#include "IScenarioDispatcher.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ViewShell {
public:
    struct OutlinerItem {
        uint64_t    id = 0;          // stable id; selection is tracked by it, not by row index
        std::string label;
        std::string panelId;         // panel opened on double-click / context menu ("" = none)
    };

    // Default placement of a panel as a fraction of the display (x, y, w, h).
    struct Rect { float x, y, w, h; };

    ViewShell();

    // Call after the ImGui context exists and before the first frame
    // (i.e. right after VkAppBase::onInit()), so imgui.ini is read into it.
    void installSettings();

    void setDispatcher(IScenarioDispatcher* d) { dispatcher_ = d; }
    void setOutlinerProvider(std::function<std::vector<OutlinerItem>()> f) { outliner_ = std::move(f); }

    // `id` is also the ImGui window title. "Command" and "Outliner" are
    // registered (visible) by the constructor; other panels default to hidden.
    void registerPanel(const std::string& id, Rect defaultRect, bool visible = false);
    bool isPanelVisible(const std::string& id) const;
    void setPanelVisible(const std::string& id, bool v);

    // Draws around the app's own panel code:
    //   if (shell.beginPanel("GSView Control")) { ...; shell.endPanel(); }
    // Returns false (and has already closed the window) when the panel is
    // hidden or collapsed, so endPanel() is only called after a true return.
    bool beginPanel(const std::string& id);
    void endPanel();

    // While a scenario runs, manual input is disabled (viewing stays possible).
    void setScenarioActive(bool active) { scenarioActive_ = active; }

    // Call once per frame from onUpdate(), after the dispatcher processed its
    // queue and before ScenarioRunner::tick(): takes this console's own
    // responses and leaves the rest in `responses` for the scenario runner.
    void consumeResponses(std::vector<std::string>& responses) { model_.consumeResponses(responses); }

    // "View" menu; call inside the app's BeginMainMenuBar().
    void drawViewMenu();
    // Command + Outliner windows.
    void drawWindows();

    void resetLayout();
    CommandConsoleModel& model() { return model_; }

private:
    struct PanelState {
        std::string id;
        Rect        rect;
        bool        visible;
        bool        defaultVisible;
    };
    PanelState*       find(const std::string& id);
    const PanelState* find(const std::string& id) const;
    void applyPlacement(const PanelState& p);
    void drawCommand();
    void drawOutliner();
    void submitLine(const std::string& text);

    IScenarioDispatcher*    dispatcher_ = nullptr;
    std::function<std::vector<OutlinerItem>()> outliner_;
    std::vector<PanelState> panels_;
    CommandConsoleModel     model_;
    bool                    scenarioActive_     = false;
    bool                    layoutResetPending_ = false;

    char     input_[512]   = {};
    bool     focusInput_   = false;
    size_t   shownLines_   = 0;
    uint64_t selectedId_   = 0;
    bool     hasSelection_ = false;

    friend struct ViewShellIni;
};
