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

struct ImVec2Like { float x, y; };

class ViewShell {
public:
    struct OutlinerItem {
        uint64_t    id = 0;          // stable id; selection is tracked by it, not by row index
        std::string label;
        std::string panelId;         // panel opened on double-click / context menu ("" = none)
        int         selected = -1;   // -1: shell tracks the selection; 0/1: app owns it (see setSelectionHandler)
    };

    // Default placement of a panel as a fraction of the display (x, y, w, h).
    struct Rect { float x, y, w, h; };

    ViewShell();

    // Call after the ImGui context exists and before the first frame
    // (i.e. right after VkAppBase::onInit()), so imgui.ini is read into it.
    void installSettings();

    void setDispatcher(IScenarioDispatcher* d) { dispatcher_ = d; }

    // Called with the item id when an outliner row is clicked. Apps that own
    // the selection (e.g. an "active scene") set it here and report it back
    // through OutlinerItem::selected.
    void setSelectionHandler(std::function<void(uint64_t)> f) { onSelect_ = std::move(f); }
    // Called instead of toggling a registered panel when an item's panelId is
    // not a shell panel (e.g. a page of an app-specific tabbed window).
    void setOpenHandler(std::function<void(const std::string&)> f) { onOpen_ = std::move(f); }
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

    // For panels that already draw their own ImGui window (Begin(id, &visible_)):
    // the shell then only owns their visibility (View menu, imgui.ini, hidden by
    // default) and first-use placement. `id` must equal the window title.
    // Call syncing happens in drawWindows(); placement in placeBoundPanels(),
    // which must run after those panels were drawn in the same frame.
    void bindPanel(const std::string& id, Rect defaultRect,
                   std::function<bool()> getVisible, std::function<void(bool)> setVisible);
    void placeBoundPanels();
    void endPanel();

    // While a scenario runs, manual input is disabled (viewing stays possible).
    void setScenarioActive(bool active) { scenarioActive_ = active; }

    // Call once per frame from onUpdate(), after the dispatcher processed its
    // queue and before ScenarioRunner::tick(): takes this console's own
    // responses and leaves the rest in `responses` for the scenario runner.
    void consumeResponses(std::vector<std::string>& responses) { model_.consumeResponses(responses); }

    // Dispatcher the app hands to ScenarioRunner::tick(). A scenario command is
    // executed *through the Command window*: it goes through the same submit path
    // as a typed line (echo, history, send), and its response is logged there
    // before consumeResponses() passes it on to the runner.
    IScenarioDispatcher& scenarioDispatcher() { return scenarioProxy_; }

    // "View" menu; call inside the app's BeginMainMenuBar().
    void drawViewMenu();
    // Only the panel toggles (no menu of its own), for apps that merge them
    // into an existing View menu.
    void drawViewMenuItems();
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
        std::function<bool()>     getExt;   // bound (self-drawn) panels only
        std::function<void(bool)> setExt;
        bool                      extSeen = false;
    };
    PanelState*       find(const std::string& id);
    const PanelState* find(const std::string& id) const;
    struct Placement { ImVec2Like pos, size; };
    Placement placementOf(const PanelState& p) const;
    void applyPlacement(const PanelState& p);
    void syncBoundPanels();
    void drawCommand();
    void drawOutliner();
    void submitLine(const std::string& text, bool fromScenario = false);
    // A scenario command appears in the Command window's input line (as if typed and
    // sent): the text is shown for the frame in which it was issued.
    void showScenarioInput(const std::string& cmd);
    void openFromOutliner(const std::string& panelId);

    class ScenarioProxy : public IScenarioDispatcher {
    public:
        explicit ScenarioProxy(ViewShell& s) : shell_(s) {}
        void dispatch(const std::string& command) override {
            shell_.showScenarioInput(command);
            shell_.submitLine(command, true);
        }
        std::vector<std::string> collectResponses() override {
            return shell_.dispatcher_ ? shell_.dispatcher_->collectResponses() : std::vector<std::string>{};
        }
        std::vector<CommandInfo> commandCatalog() const override {
            return shell_.dispatcher_ ? shell_.dispatcher_->commandCatalog() : std::vector<CommandInfo>{};
        }
    private:
        ViewShell& shell_;
    };

    IScenarioDispatcher*    dispatcher_ = nullptr;
    std::function<std::vector<OutlinerItem>()> outliner_;
    std::function<void(uint64_t)>              onSelect_;
    std::function<void(const std::string&)>    onOpen_;
    std::vector<PanelState> panels_;
    CommandConsoleModel     model_;
    ScenarioProxy           scenarioProxy_{*this};
    bool                    scenarioActive_     = false;
    bool                    resetRequested_     = false;  // set by resetLayout()
    bool                    layoutResetPending_ = false;  // true for the one frame the reset is applied

    char     input_[512]   = {};
    bool     focusInput_   = false;
    bool     scenarioEcho_ = false;   // input_ currently holds a scenario command
    size_t   shownLines_   = 0;
    uint64_t selectedId_   = 0;
    bool     hasSelection_ = false;

    friend struct ViewShellIni;
};
