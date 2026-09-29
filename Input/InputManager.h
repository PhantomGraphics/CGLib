#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "GameAction.h"

#include <array>

namespace Phantom::Input {

// Holds the value of each named game action and the gamepad state.
//
// What a key, mouse button or stick *means* is not decided here: the app's input router maps
// physical inputs to actions through a keymap (bind key.W action=move_forward) and writes the
// result with setActionValue() once per frame. This class only stores those values, lets tests
// force them (injectAction), and polls the gamepad (update()) so the router can read raw pad
// buttons and axes (padButton()/padAxis()).
//
// MoveForward/MoveBack (and MoveLeft/MoveRight, and the CamRight/CamLeft/CamUp/CamDown right-
// stick pairs) are separate actions rather than one signed axis each, so getAxis() always
// returns a magnitude in [0,1] -- the caller subtracts pairs itself.
class InputManager {
public:
    static constexpr size_t kActionCount = 10;

    // Polls glfwGetGamepadState(). Call once per frame before the router reads pad state.
    void update();

    bool  isPressed(GameAction action) const;
    float getAxis(GameAction action) const;

    // Written by the input router (already bound, scaled, clamped to [0,1]).
    void setActionValue(GameAction action, float value) { values_[static_cast<size_t>(action)] = value; }

    // Raw gamepad state (GLFW_GAMEPAD_BUTTON_* / GLFW_GAMEPAD_AXIS_*). Axes have a 0.2 dead zone applied.
    bool  padButton(int glfwButton) const;
    float padAxis(int glfwAxis) const;

    // Forces isPressed()/getAxis() for this action to a fixed digital value (1.f/0.f for
    // getAxis) until injectAction() is called again for it or clearInjectedActions() runs --
    // lets a scenario test (no real keyboard/gamepad) drive character input deterministically.
    void injectAction(GameAction action, bool pressed) { injected_[static_cast<size_t>(action)] = pressed ? 1 : 0; }
    void clearInjectedActions() { injected_.fill(-1); }

private:
    // -1 = not injected (use the routed value); 0/1 = forced false/true.
    std::array<int8_t, kActionCount> injected_ = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<float, kActionCount>  values_   = {};

    bool             hasGamepad_   = false;
    GLFWgamepadstate gamepadState_ = {};
};

} // namespace Phantom::Input
