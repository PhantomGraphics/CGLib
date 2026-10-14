#pragma once

#include "Input.h"

#include <cstddef>
#include <utility>
#include <string>
#include <vector>

namespace Phantom::Editor {

// Binding kinds (docs/todo/PLAN_universe_command_ui.md §4.2):
//   Trigger: runs `command` once when the input fires (press by default, or release with state=release)
//   Delta:   runs `command` with $dx / $dy / $value replaced, for drags, cursor moves, wheel, axes
//   Action:  maps the input onto a named action value that a controller reads every frame
enum class BindingType : uint8_t { Trigger, Delta, Action };

struct Binding {
    std::string context; // "global", "view", "play", ...
    InputSpec input;
    BindingType type = BindingType::Trigger;
    std::string command; // Trigger / Delta
    std::string action;  // Action
    bool invert = false; // Action: use the negative direction of an axis
    double scale = 1.0;  // Delta: multiplies dx/dy/value. Action: multiplies the axis value
    bool onRelease = false; // Trigger: fire on release instead of press
};

class BindingTable {
public:
    // Adds a binding, replacing an existing one with the same context + input (+ action for
    // Action bindings, whose input may drive several actions, e.g. one stick axis -> forward/back).
    void bind(Binding binding);
    // Removes every binding of `input` in `context`. Returns how many were removed.
    size_t unbind(const std::string& context, const InputSpec& input);
    void clear() { bindings_.clear(); }
    const std::vector<Binding>& all() const { return bindings_; }
    // Replaces the whole table (undo of a keymap change, keymap load).
    void assign(std::vector<Binding> bindings) { bindings_ = std::move(bindings); }

    // Bindings that fire for `event`. Modifiers must match exactly. The active context is
    // searched first; "global" is used only when the active context has no match.
    // Pass an empty activeContext to search "global" only.
    std::vector<const Binding*> resolve(const std::string& activeContext, const InputSpec& event) const;

    // Everything bound to `input` in any context, for "bindings key.W".
    std::vector<const Binding*> find(const InputSpec& input) const;

private:
    std::vector<Binding> bindings_;
};

// Replaces $dx, $dy and $value in a Delta/Trigger command (each multiplied by binding.scale).
std::string expandCommand(const Binding& binding, double dx, double dy, double value);

// --- Keymap files (.ukeymap): the same one-command-per-line form as .phs ---------------------
//   context <name>
//   bind <input> "<command>" [scale=<n>] [state=press|release]
//   bind <input> action=<name> [invert=1] [scale=<n>]
//   unbind <input>

// Executes one `bind ...` command (tokens[0] == "bind") in `context`.
bool executeBind(BindingTable& table, const std::string& context,
    const std::vector<std::string>& tokens, const std::vector<bool>& quotedAtStart, std::string& error);

// Applies one keymap line; `context` is read and updated by `context <name>` lines.
// Blank and comment-only lines succeed without effect.
bool applyKeymapLine(BindingTable& table, std::string& context, const std::string& line, std::string& error);

// Applies a whole keymap. On failure nothing after the bad line is applied and `error` is
// prefixed with "line N: ". Returns true when every line applied.
bool loadKeymap(BindingTable& table, const std::vector<std::string>& lines, std::string& error);

// Serialises the table back to keymap text (loadKeymap(saveKeymap(t)) reproduces t).
std::string saveKeymap(const BindingTable& table);

} // namespace Phantom::Editor
