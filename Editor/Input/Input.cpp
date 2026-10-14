#include "Input.h"

#include "../Commands/Tokenizer.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace Phantom::Editor {
namespace {

bool isName(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '_' || c == '.';
    });
}

bool isMouseButton(const std::string& s) { return s == "left" || s == "right" || s == "middle"; }

bool isPadAxis(const std::string& s) {
    return s == "lstick.x" || s == "lstick.y" || s == "rstick.x" || s == "rstick.y" ||
           s == "ltrigger" || s == "rtrigger";
}

bool isPadButton(const std::string& s) {
    static const char* names[] = {"a", "b", "x", "y", "lbumper", "rbumper", "back", "start", "guide",
        "lstickbutton", "rstickbutton", "dpad_up", "dpad_down", "dpad_left", "dpad_right"};
    return std::any_of(std::begin(names), std::end(names), [&](const char* n) { return s == n; });
}

const char* modifierName(uint8_t bit) {
    switch (bit) {
    case ModCtrl: return "ctrl";
    case ModShift: return "shift";
    case ModAlt: return "alt";
    case ModSuper: return "super";
    }
    return "";
}

bool endsWith(const std::string& s, const char* suffix) {
    const std::string x(suffix);
    return s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

} // namespace

bool parseInput(const std::string& text, InputSpec& out, std::string& error) {
    out = InputSpec{};
    error.clear();
    if (text.empty()) { error = "empty input"; return false; }

    // Modifiers are everything before the last '+' that is not part of the key name itself
    // ("key.+" is not supported; use a named key instead).
    std::string rest = text;
    for (;;) {
        const size_t plus = rest.find('+');
        if (plus == std::string::npos || plus + 1 >= rest.size()) break;
        const std::string mod = toLower(rest.substr(0, plus));
        uint8_t bit = ModNone;
        if (mod == "ctrl") bit = ModCtrl;
        else if (mod == "shift") bit = ModShift;
        else if (mod == "alt") bit = ModAlt;
        else if (mod == "super") bit = ModSuper;
        else { error = "unknown modifier '" + rest.substr(0, plus) + "'"; return false; }
        if (out.modifiers & bit) { error = "duplicate modifier '" + mod + "'"; return false; }
        out.modifiers |= bit;
        rest = rest.substr(plus + 1);
    }

    const size_t dot = rest.find('.');
    if (dot == std::string::npos) { error = "expected key.<name>, mouse.<...> or pad.<...>: " + text; return false; }
    const std::string device = toLower(rest.substr(0, dot));
    const std::string body = rest.substr(dot + 1);
    if (!isName(body)) { error = "invalid input name: " + text; return false; }
    const std::string lower = toLower(body);

    if (device == "key") {
        out.device = InputDevice::Key;
        out.kind = InputKind::Press;
        out.name = lower;
        out.display = body;
        return true;
    }
    if (device == "mouse") {
        out.device = InputDevice::Mouse;
        if (lower == "move") { out.kind = InputKind::Move; out.name = "move"; out.display = "move"; return true; }
        if (lower == "wheel") { out.kind = InputKind::Wheel; out.name = "wheel"; out.display = "wheel"; return true; }
        std::string button = lower;
        out.kind = InputKind::Press;
        if (endsWith(lower, ".click")) { out.kind = InputKind::Click; button = lower.substr(0, lower.size() - 6); }
        else if (endsWith(lower, ".drag")) { out.kind = InputKind::Drag; button = lower.substr(0, lower.size() - 5); }
        if (!isMouseButton(button)) { error = "unknown mouse input: " + text; return false; }
        out.name = button;
        out.display = button;
        return true;
    }
    if (device == "pad") {
        out.device = InputDevice::Pad;
        if (isPadAxis(lower)) out.kind = InputKind::Axis;
        else if (isPadButton(lower)) out.kind = InputKind::Press;
        else { error = "unknown pad input: " + text; return false; }
        out.name = lower;
        out.display = lower;
        return true;
    }
    error = "unknown device '" + rest.substr(0, dot) + "' (expected key, mouse or pad)";
    return false;
}

std::string formatInput(const InputSpec& input) {
    std::string out;
    for (uint8_t bit : {ModCtrl, ModShift, ModAlt, ModSuper})
        if (input.modifiers & bit) { out += modifierName(bit); out += '+'; }
    switch (input.device) {
    case InputDevice::Key: out += "key."; out += input.display; break;
    case InputDevice::Pad: out += "pad."; out += input.display; break;
    case InputDevice::Mouse:
        out += "mouse.";
        out += input.display;
        if (input.kind == InputKind::Click) out += ".click";
        else if (input.kind == InputKind::Drag) out += ".drag";
        break;
    }
    return out;
}

} // namespace Phantom::Editor
