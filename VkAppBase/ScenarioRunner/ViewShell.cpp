#include "ViewShell.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

// ---- imgui.ini section ----------------------------------------------------

struct ViewShellIni {
    static ViewShell* shell(ImGuiSettingsHandler* h) { return static_cast<ViewShell*>(h->UserData); }

    static void* readOpen(ImGuiContext*, ImGuiSettingsHandler* h, const char* name) {
        return std::strcmp(name, "Panels") == 0 ? h->UserData : nullptr;
    }
    static void readLine(ImGuiContext*, ImGuiSettingsHandler* h, void*, const char* line) {
        const char* eq = std::strrchr(line, '=');
        if (!eq || eq == line) return;
        const std::string id(line, eq);
        if (auto* p = shell(h)->find(id)) p->visible = std::atoi(eq + 1) != 0;
    }
    static void writeAll(ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* out) {
        out->appendf("[%s][Panels]\n", h->TypeName);
        for (const auto& p : shell(h)->panels_) out->appendf("%s=%d\n", p.id.c_str(), p.visible ? 1 : 0);
        out->append("\n");
    }
};

ViewShell::ViewShell() {
    registerPanel("Outliner", {0.f, 0.f, 0.20f, 0.66f}, true);
    registerPanel("Command", {0.f, 0.68f, 0.62f, 0.32f}, true);
}

void ViewShell::installSettings() {
    ImGuiSettingsHandler h;
    h.TypeName   = "ViewShell";
    h.TypeHash   = ImHashStr("ViewShell");
    h.ReadOpenFn = ViewShellIni::readOpen;
    h.ReadLineFn = ViewShellIni::readLine;
    h.WriteAllFn = ViewShellIni::writeAll;
    h.UserData   = this;
    ImGui::AddSettingsHandler(&h);
}

// ---- panels ---------------------------------------------------------------

ViewShell::PanelState* ViewShell::find(const std::string& id) {
    for (auto& p : panels_) if (p.id == id) return &p;
    return nullptr;
}
const ViewShell::PanelState* ViewShell::find(const std::string& id) const {
    for (const auto& p : panels_) if (p.id == id) return &p;
    return nullptr;
}

void ViewShell::registerPanel(const std::string& id, Rect r, bool visible) {
    if (auto* p = find(id)) { p->rect = r; p->defaultVisible = visible; return; }
    panels_.push_back({id, r, visible, visible});
}

bool ViewShell::isPanelVisible(const std::string& id) const {
    const auto* p = find(id);
    return p && p->visible;
}

void ViewShell::setPanelVisible(const std::string& id, bool v) {
    if (auto* p = find(id); p && p->visible != v) {
        p->visible = v;
        ImGui::MarkIniSettingsDirty();
    }
}

void ViewShell::applyPlacement(const PanelState& p) {
    const ImVec2 d = ImGui::GetIO().DisplaySize;
    const float menuH = ImGui::GetFrameHeight();
    const float areaH = std::max(1.f, d.y - menuH);
    // Never smaller than a usable minimum, never larger than the window, so a
    // small window still shows an input line and a list.
    const float w = std::min(std::max(p.rect.w * d.x, 200.f), d.x);
    const float h = std::min(std::max(p.rect.h * areaH, 120.f), areaH);
    const float x = std::min(p.rect.x * d.x, std::max(0.f, d.x - w));
    const float y = menuH + std::min(p.rect.y * areaH, std::max(0.f, areaH - h));
    // FirstUseEver: a position/size already in imgui.ini wins; a layout reset forces ours.
    const ImGuiCond cond = layoutResetPending_ ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(x, y), cond);
    ImGui::SetNextWindowSize(ImVec2(w, h), cond);
}

bool ViewShell::beginPanel(const std::string& id) {
    const PanelState* p = find(id);
    if (!p || !p->visible) return false;
    applyPlacement(*p);
    bool open = true;
    const bool shown = ImGui::Begin(id.c_str(), &open);
    if (!open) setPanelVisible(id, false);
    if (!shown) ImGui::End();
    return shown;
}

void ViewShell::endPanel() {
    ImGui::End();
}

void ViewShell::resetLayout() {
    for (auto& p : panels_) p.visible = p.defaultVisible;
    layoutResetPending_ = true;
    ImGui::MarkIniSettingsDirty();
}

void ViewShell::drawViewMenu() {
    if (!ImGui::BeginMenu("View")) return;
    for (auto& p : panels_) {
        bool v = p.visible;
        if (ImGui::MenuItem(p.id.c_str(), nullptr, &v)) setPanelVisible(p.id, v);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Layout")) resetLayout();
    ImGui::EndMenu();
}

// ---- Command window -------------------------------------------------------

void ViewShell::submitLine(const std::string& text) {
    std::string cmd;
    const auto catalog = dispatcher_ ? dispatcher_->commandCatalog() : std::vector<CommandInfo>{};
    if (model_.submit(text, catalog, cmd) == CommandConsoleModel::SubmitResult::Dispatch && dispatcher_)
        dispatcher_->dispatch(cmd);
}

namespace {
struct InputCtx {
    CommandConsoleModel*     model;
    std::vector<CommandInfo> catalog;
};

int inputCallback(ImGuiInputTextCallbackData* data) {
    auto* c = static_cast<InputCtx*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        const std::string s = data->EventKey == ImGuiKey_UpArrow ? c->model->historyPrev()
                                                                  : c->model->historyNext();
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, s.c_str());
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        const std::string text(data->Buf, data->BufTextLen);
        const auto cands = CommandConsoleModel::complete(text, c->catalog);
        if (cands.empty()) return 0;
        const std::string prefix = CommandConsoleModel::commonPrefix(cands);
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, prefix.c_str());
        if (cands.size() > 1) {
            std::string row = "  ";
            for (const auto& n : cands) row += n + "  ";
            c->model->addInfo(row);
        }
    }
    return 0;
}
} // namespace

void ViewShell::drawCommand() {
    if (!beginPanel("Command")) return;

    const float footer = ImGui::GetFrameHeightWithSpacing() * (scenarioActive_ ? 2.f : 1.f) + 2.f;
    if (ImGui::BeginChild("##log", ImVec2(0, -footer), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& l : model_.lines()) {
            switch (l.kind) {
            case CommandConsoleModel::Line::Kind::Input:
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.f, 1.f), "%s", l.text.c_str()); break;
            case CommandConsoleModel::Line::Kind::Error:
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", l.text.c_str()); break;
            case CommandConsoleModel::Line::Kind::Info:
                ImGui::TextDisabled("%s", l.text.c_str()); break;
            default:
                ImGui::TextUnformatted(l.text.c_str()); break;
            }
        }
        if (model_.lines().size() != shownLines_) {
            shownLines_ = model_.lines().size();
            ImGui::SetScrollHereY(1.f);
        }
    }
    ImGui::EndChild();

    if (scenarioActive_) ImGui::TextDisabled("Scenario running - manual input disabled");
    ImGui::BeginDisabled(scenarioActive_);
    InputCtx ctx{&model_, dispatcher_ ? dispatcher_->commandCatalog() : std::vector<CommandInfo>{}};
    ImGui::SetNextItemWidth(-1.f);
    if (focusInput_) { ImGui::SetKeyboardFocusHere(); focusInput_ = false; }
    constexpr ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;
    if (ImGui::InputTextWithHint("##cmd", "command (help, Tab = complete)", input_, sizeof(input_),
                                 flags, inputCallback, &ctx)) {
        submitLine(input_);
        input_[0] = '\0';
        focusInput_ = true;
    }
    ImGui::EndDisabled();
    endPanel();
}

// ---- Outliner -------------------------------------------------------------

void ViewShell::drawOutliner() {
    if (!beginPanel("Outliner")) return;

    const auto items = outliner_ ? outliner_() : std::vector<OutlinerItem>{};
    // A selected object that disappeared (unloaded / reloaded) is deselected.
    if (hasSelection_ && std::none_of(items.begin(), items.end(),
                                      [&](const OutlinerItem& i) { return i.id == selectedId_; }))
        hasSelection_ = false;

    if (items.empty()) ImGui::TextDisabled("(nothing loaded)");
    for (const auto& it : items) {
        ImGui::PushID(static_cast<int>(it.id & 0x7fffffff));
        const bool sel = hasSelection_ && selectedId_ == it.id;
        if (ImGui::Selectable(it.label.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedId_   = it.id;
            hasSelection_ = true;
            if (ImGui::IsMouseDoubleClicked(0) && !it.panelId.empty()) setPanelVisible(it.panelId, true);
        }
        if (ImGui::BeginPopupContextItem()) {
            if (!it.panelId.empty() && ImGui::MenuItem("Open properties")) setPanelVisible(it.panelId, true);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    endPanel();
}

void ViewShell::drawWindows() {
    drawOutliner();
    drawCommand();
    layoutResetPending_ = false;
}
