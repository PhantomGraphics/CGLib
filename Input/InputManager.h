#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "GameAction.h"

#include <array>

namespace Phantom::Input {

// Polls GLFW keyboard + gamepad state once per frame (update()) rather than reacting to
// key-down/key-up callbacks, so "is this action held right now" comes for free -- a character
// controller needs continuous per-frame movement state, which VulkanWindow::onKey's
// edge-triggered callback doesn't suit. Mouse-drag camera input is handled separately
// (getMouseDelta()/isRightDragging()) rather than as a GameAction: it's a raw per-frame delta
// for FollowCamera's orbit, not a discrete action.
//
// MoveForward/MoveBack (and MoveLeft/MoveRight, and the CamRight/CamLeft/CamUp/CamDown right-
// stick pairs) are modeled as separate actions rather than one signed axis each, so getAxis()
// always returns a non-negative magnitude in [0,1] -- the caller subtracts pairs itself, e.g.
// moveZ = getAxis(MoveBack) - getAxis(MoveForward).
class InputManager {
public:
    void init(GLFWwindow* window) { window_ = window; }

    // Poll glfwGetKey()/glfwGetGamepadState() and finalize this frame's mouse delta.
    // Call once per frame before reading any action/axis/mouse-delta below.
    void update();

    bool  isPressed(GameAction action) const;
    float getAxis(GameAction action) const;

    glm::vec2 getMouseDelta()    const { return mouseDelta_; }
    bool      isRightDragging()  const { return rightDown_;  }

    // Connect to VulkanWindow::onMouseButton / onCursorPos.
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double x, double y);

    // Forces isPressed()/getAxis() for this action to a fixed digital value (1.f/0.f for
    // getAxis) until injectAction() is called again for it or clearInjectedActions() runs --
    // lets a scenario test (no real keyboard/gamepad) drive character input deterministically
    // (see CGApp/Universe/UniverseCommandDispatcher's "InjectInput" command).
    void injectAction(GameAction action, bool pressed) { injected_[static_cast<size_t>(action)] = pressed ? 1 : 0; }
    void clearInjectedActions() { injected_.fill(-1); }

private:
    GLFWwindow* window_ = nullptr;

    // -1 = not injected (read real hardware state); 0/1 = forced false/true.
    std::array<int8_t, 10> injected_ = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    glm::vec2 lastCursor_       = { 0.f, 0.f };
    glm::vec2 accumMouseDelta_  = { 0.f, 0.f }; // accumulated since the last update()
    glm::vec2 mouseDelta_       = { 0.f, 0.f }; // finalized by update(), read by getMouseDelta()
    bool      rightDown_        = false;
    bool      firstCursorSample_ = true;

    bool             hasGamepad_   = false;
    GLFWgamepadstate gamepadState_ = {};
};

} // namespace Phantom::Input
