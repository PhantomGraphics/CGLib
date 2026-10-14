#include "Bindings.h"

#include "../Commands/Tokenizer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace Phantom::Editor {
namespace {

bool sameSlot(const Binding& a, const Binding& b) {
    if (a.context != b.context || !(a.input == b.input)) return false;
    if (a.type == BindingType::Action || b.type == BindingType::Action)
        return a.type == b.type && a.action == b.action;
    return a.onRelease == b.onRelease;
}

std::string formatValue(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size())
        s.replace(pos, from.size(), to);
}

bool parseNumber(const std::string& text, double& out) {
    char* end = nullptr;
    out = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0' && std::isfinite(out);
}

bool parseBool(const std::string& text, bool& out) {
    const std::string v = toLower(text);
    if (v == "1" || v == "true") { out = true; return true; }
    if (v == "0" || v == "false") { out = false; return true; }
    return false;
}

} // namespace

void BindingTable::bind(Binding binding) {
    const auto it = std::find_if(bindings_.begin(), bindings_.end(),
        [&](const Binding& b) { return sameSlot(b, binding); });
    if (it != bindings_.end()) *it = std::move(binding);
    else bindings_.push_back(std::move(binding));
}

size_t BindingTable::unbind(const std::string& context, const InputSpec& input) {
    const auto first = std::remove_if(bindings_.begin(), bindings_.end(),
        [&](const Binding& b) { return b.context == context && b.input == input; });
    const size_t removed = static_cast<size_t>(bindings_.end() - first);
    bindings_.erase(first, bindings_.end());
    return removed;
}

std::vector<const Binding*> BindingTable::resolve(const std::string& activeContext, const InputSpec& event) const {
    const auto collect = [&](const std::string& context) {
        std::vector<const Binding*> found;
        for (const auto& b : bindings_)
            if (b.context == context && b.input == event) found.push_back(&b);
        return found;
    };
    if (!activeContext.empty() && activeContext != "global") {
        auto found = collect(activeContext);
        if (!found.empty()) return found;
    }
    return collect("global");
}

std::vector<const Binding*> BindingTable::find(const InputSpec& input) const {
    std::vector<const Binding*> found;
    for (const auto& b : bindings_)
        if (b.input == input) found.push_back(&b);
    return found;
}

std::string expandCommand(const Binding& binding, double dx, double dy, double value) {
    std::string out = binding.command;
    replaceAll(out, "$dx", formatValue(dx * binding.scale));
    replaceAll(out, "$dy", formatValue(dy * binding.scale));
    replaceAll(out, "$value", formatValue(value * binding.scale));
    return out;
}

bool executeBind(BindingTable& table, const std::string& context,
    const std::vector<std::string>& tokens, const std::vector<bool>& quotedAtStart, std::string& error)
{
    error.clear();
    if (tokens.size() < 3) { error = "usage: bind <input> \"<command>\" | bind <input> action=<name>"; return false; }
    Binding binding;
    binding.context = context;
    if (!parseInput(tokens[1], binding.input, error)) return false;

    bool hasAction = false, hasState = false, hasInvert = false, hasScale = false;
    for (size_t i = 2; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        const size_t eq = tok.find('=');
        const bool quoted = i < quotedAtStart.size() && quotedAtStart[i];
        const std::string key = eq == std::string::npos ? std::string() : tok.substr(0, eq);
        if (!quoted && (key == "action" || key == "invert" || key == "scale" || key == "state")) {
            const std::string value = tok.substr(eq + 1);
            if (key == "action") {
                if (value.empty()) { error = "action= needs a name"; return false; }
                binding.action = value; hasAction = true;
            } else if (key == "invert") {
                if (!parseBool(value, binding.invert)) { error = "invalid invert value: " + value; return false; }
                hasInvert = true;
            } else if (key == "scale") {
                if (!parseNumber(value, binding.scale)) { error = "invalid scale value: " + value; return false; }
                hasScale = true;
            } else {
                if (value == "press") binding.onRelease = false;
                else if (value == "release") binding.onRelease = true;
                else { error = "state must be press or release"; return false; }
                hasState = true;
            }
        } else if (binding.command.empty() && !hasAction) {
            binding.command = tok;
        } else {
            error = "unexpected argument: " + tok;
            return false;
        }
    }

    if (hasAction) {
        if (!binding.command.empty()) { error = "a binding has either a command or action=, not both"; return false; }
        if (hasState) { error = "state= applies to command bindings only"; return false; }
        binding.type = BindingType::Action;
    } else {
        if (binding.command.empty()) { error = "missing command"; return false; }
        if (hasInvert) { error = "invert= applies to action bindings only"; return false; }
        const InputKind kind = binding.input.kind;
        binding.type = (kind == InputKind::Drag || kind == InputKind::Move ||
                        kind == InputKind::Wheel || kind == InputKind::Axis)
            ? BindingType::Delta : BindingType::Trigger;
        if (binding.type == BindingType::Delta && hasState) {
            error = "state= applies to press/click bindings only";
            return false;
        }
        if (binding.type == BindingType::Trigger && hasScale) {
            error = "scale= applies to delta and action bindings only";
            return false;
        }
    }
    table.bind(std::move(binding));
    return true;
}

bool applyKeymapLine(BindingTable& table, std::string& context, const std::string& line, std::string& error) {
    error.clear();
    ParsedCommand parsed;
    if (!tokenize(line, parsed, error)) return false;
    if (parsed.tokens.empty()) return true;
    const std::string verb = toLower(parsed.tokens[0]);
    if (verb == "context") {
        if (parsed.tokens.size() != 2 || parsed.tokens[1].empty()) { error = "usage: context <name>"; return false; }
        context = toLower(parsed.tokens[1]);
        return true;
    }
    if (context.empty()) { error = "no context selected (add a 'context <name>' line first)"; return false; }
    if (verb == "bind") return executeBind(table, context, parsed.tokens, parsed.quotedAtStart, error);
    if (verb == "unbind") {
        if (parsed.tokens.size() != 2) { error = "usage: unbind <input>"; return false; }
        InputSpec input;
        if (!parseInput(parsed.tokens[1], input, error)) return false;
        table.unbind(context, input);
        return true;
    }
    error = "unknown keymap command: " + parsed.tokens[0];
    return false;
}

bool loadKeymap(BindingTable& table, const std::vector<std::string>& lines, std::string& error) {
    std::string context;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string lineError;
        if (!applyKeymapLine(table, context, lines[i], lineError)) {
            error = "line " + std::to_string(i + 1) + ": " + lineError;
            return false;
        }
    }
    error.clear();
    return true;
}

std::string saveKeymap(const BindingTable& table) {
    std::string out;
    std::string context;
    // Group by context, keeping first-appearance order of contexts and bindings within each.
    std::vector<std::string> contexts;
    for (const auto& b : table.all())
        if (std::find(contexts.begin(), contexts.end(), b.context) == contexts.end()) contexts.push_back(b.context);
    for (const auto& ctx : contexts) {
        if (!out.empty()) out += "\n";
        out += "context " + ctx + "\n";
        for (const auto& b : table.all()) {
            if (b.context != ctx) continue;
            out += "bind " + formatInput(b.input);
            if (b.type == BindingType::Action) {
                out += " action=" + b.action;
                if (b.invert) out += " invert=1";
            } else {
                out += " " + quoteToken(b.command);
                if (b.onRelease) out += " state=release";
            }
            if (b.type != BindingType::Trigger && b.scale != 1.0) out += " scale=" + formatValue(b.scale);
            out += "\n";
        }
    }
    return out;
}

} // namespace Phantom::Editor
