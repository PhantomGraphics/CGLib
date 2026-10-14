#pragma once

#include <cstdint>
#include <string>

namespace Phantom::Editor {

// Input notation (docs/todo/PLAN_universe_command_ui.md §4.1):
//   key.W  key.Space  key.F5                 keys
//   ctrl+key.S  shift+key.W                  modifiers (ctrl / shift / alt / super)
//   mouse.left  mouse.right  mouse.middle    buttons (press)
//   mouse.left.click  mouse.left.drag        click (press+release) / drag (button held while moving)
//   mouse.move  mouse.wheel                  cursor delta / wheel
//   pad.a  pad.lbumper                       gamepad buttons
//   pad.lstick.x  pad.rstick.y  pad.ltrigger gamepad axes

enum class InputDevice : uint8_t { Key, Mouse, Pad };

enum class InputKind : uint8_t {
    Press, // key, mouse button or pad button going down/up
    Click, // mouse button pressed and released without moving
    Drag,  // mouse button held while the cursor moves
    Move,  // cursor movement
    Wheel, // scroll wheel
    Axis   // analog pad axis or trigger
};

enum Modifier : uint8_t { ModNone = 0, ModCtrl = 1, ModShift = 2, ModAlt = 4, ModSuper = 8 };

struct InputSpec {
    uint8_t modifiers = ModNone;
    InputDevice device = InputDevice::Key;
    InputKind kind = InputKind::Press;
    std::string name;    // canonical: lower case ("w", "left", "lstick.x")
    std::string display; // spelling used for formatting ("W", "left", "lstick.x")

    bool operator==(const InputSpec& o) const {
        return modifiers == o.modifiers && device == o.device && kind == o.kind && name == o.name;
    }
};

bool parseInput(const std::string& text, InputSpec& out, std::string& error);
std::string formatInput(const InputSpec& input);

} // namespace Phantom::Editor
