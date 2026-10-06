#pragma once
// ImGui-free state of the command window: history, pending-response
// bookkeeping, local commands (help / clear) and name completion.
// Kept header-only and dependency-free so it can be unit tested.
#include "CommandInfo.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

class CommandConsoleModel {
public:
    struct Line {
        enum class Kind { Input, Output, Error, Info } kind;
        std::string text;
    };

    enum class SubmitResult { Ignored, Local, Dispatch };

    // Handles one entered line. Dispatch: `outCommand` must be sent to the
    // app's dispatcher (the line is trimmed, otherwise passed through as-is, so
    // a scenario "command" string works unchanged). Local: help/clear, already
    // answered. Ignored: blank.
    //
    // `fromScenario`: the line comes from a running scenario. It takes exactly the
    // same path (echo, history, pending bookkeeping), except that the console-local
    // commands (help/clear) are not interpreted -- everything reaches the
    // dispatcher -- and its response is handed on to the scenario runner.
    SubmitResult submit(const std::string& text, const std::vector<CommandInfo>& catalog,
                        std::string& outCommand, bool fromScenario = false) {
        const std::string cmd = trim(text);
        if (cmd.empty()) return SubmitResult::Ignored;
        if (history_.empty() || history_.back() != cmd) history_.push_back(cmd);
        historyPos_ = history_.size();
        push({Line::Kind::Input, "> " + cmd});

        if (!fromScenario && cmd == "clear") {
            lines_.clear();
            return SubmitResult::Local;
        }
        if (!fromScenario && (cmd == "help" || cmd.rfind("help ", 0) == 0)) {
            addHelp(trim(cmd.substr(4)), catalog);
            return SubmitResult::Local;
        }
        outCommand = cmd;
        pending_.push_back(fromScenario);
        return SubmitResult::Dispatch;
    }

    // Takes the responses of the commands sent through this console (the oldest
    // `pending()` ones, since the dispatcher answers in order), logs them, and
    // leaves in `responses` only what belongs to the scenario runner: the
    // answers to scenario-issued commands (in order), then any unclaimed rest.
    void consumeResponses(std::vector<std::string>& responses) {
        const size_t n = std::min(pending_.size(), responses.size());
        std::vector<std::string> forScenario;
        for (size_t i = 0; i < n; ++i) {
            const bool err = isError(responses[i]);
            push({err ? Line::Kind::Error : Line::Kind::Output, responses[i]});
            if (pending_[i]) forScenario.push_back(std::move(responses[i]));
        }
        responses.erase(responses.begin(), responses.begin() + static_cast<std::ptrdiff_t>(n));
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(n));
        responses.insert(responses.begin(), forScenario.begin(), forScenario.end());
    }

    size_t pending() const { return pending_.size(); }
    const std::vector<Line>& lines() const { return lines_; }
    void addInfo(const std::string& s) { push({Line::Kind::Info, s}); }

    // Up/Down arrow history. Returns the text for the input box.
    std::string historyPrev() {
        if (history_.empty()) return {};
        if (historyPos_ > 0) --historyPos_;
        return history_[historyPos_];
    }
    std::string historyNext() {
        if (historyPos_ + 1 >= history_.size()) {
            historyPos_ = history_.size();
            return {};
        }
        return history_[++historyPos_];
    }

    // Catalog names starting with the command-name part of `text` (case
    // sensitive, like the dispatchers). Names are only offered while the cursor
    // is still in the name, i.e. before the first ':'.
    static std::vector<std::string> complete(const std::string& text,
                                             const std::vector<CommandInfo>& catalog) {
        std::vector<std::string> out;
        if (text.find(':') != std::string::npos) return out;
        for (const auto& c : catalog)
            if (c.name.rfind(text, 0) == 0) out.push_back(c.name);
        std::sort(out.begin(), out.end());
        return out;
    }

    static std::string commonPrefix(const std::vector<std::string>& v) {
        if (v.empty()) return {};
        std::string p = v.front();
        for (const auto& s : v) {
            size_t i = 0;
            while (i < p.size() && i < s.size() && p[i] == s[i]) ++i;
            p.resize(i);
        }
        return p;
    }

    // "Error:..." (most apps) or "ERROR:..." (AnimationView).
    static bool isError(const std::string& r) {
        if (r.size() < 5) return false;
        for (size_t i = 0; i < 5; ++i)
            if (std::tolower(static_cast<unsigned char>(r[i])) != "error"[i]) return false;
        return true;
    }

    static std::string trim(const std::string& s) {
        const char* ws = " \t\r\n";
        const size_t b = s.find_first_not_of(ws);
        if (b == std::string::npos) return {};
        return s.substr(b, s.find_last_not_of(ws) - b + 1);
    }

private:
    // Scenarios can issue thousands of commands; keep the log bounded.
    void push(Line l) {
        lines_.push_back(std::move(l));
        constexpr size_t kMaxLines = 2000;
        if (lines_.size() > kMaxLines)
            lines_.erase(lines_.begin(), lines_.begin() + static_cast<std::ptrdiff_t>(lines_.size() - kMaxLines));
    }

    void addHelp(const std::string& topic, const std::vector<CommandInfo>& catalog) {
        if (topic.empty()) {
            addInfo("Type a command exactly as in a scenario's \"command\" field, e.g. GetStatus or SetRenderMode:GaussianPoint.");
            addInfo("help <name> shows one command, clear empties this log, Tab completes names, Up/Down browse history.");
            if (catalog.empty()) { addInfo("(this app publishes no command list)"); return; }
            std::vector<std::string> names;
            for (const auto& c : catalog) names.push_back(c.name);
            std::sort(names.begin(), names.end());
            std::string row;
            for (const auto& n : names) {
                if (!row.empty() && row.size() + n.size() > 100) { addInfo(row); row.clear(); }
                row += (row.empty() ? "" : "  ") + n;
            }
            if (!row.empty()) addInfo(row);
            return;
        }
        for (const auto& c : catalog) {
            if (c.name != topic) continue;
            addInfo(c.name + (c.args.empty() ? "" : ":" + c.args));
            if (!c.help.empty()) addInfo("  " + c.help);
            return;
        }
        push({Line::Kind::Error, "Error:no help for unknown command " + topic});
    }

    std::vector<Line>        lines_;
    std::vector<std::string> history_;
    size_t                   historyPos_ = 0;
    std::deque<bool>         pending_;   // per in-flight command: issued by a scenario?
};
