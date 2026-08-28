#include "InputManager.h"

#include <algorithm>
#include <cmath>

namespace Phantom::Input {

namespace {

constexpr float kDeadzone = 0.2f;

float applyDeadzone(float v)
{
    return (std::fabs(v) < kDeadzone) ? 0.f : v;
}

} // namespace

void InputManager::update()
{
    hasGamepad_ = glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState_) == GLFW_TRUE;

    mouseDelta_      = accumMouseDelta_;
    accumMouseDelta_ = { 0.f, 0.f };
}

float InputManager::getAxis(GameAction action) const
{
    const int8_t injected = injected_[static_cast<size_t>(action)];
    if (injected >= 0) return injected != 0 ? 1.f : 0.f;

    const auto keyDown = [this](int key) {
        return window_ && glfwGetKey(window_, key) == GLFW_PRESS;
    };
    const auto axisPositive = [&](int glfwAxis) {
        return hasGamepad_ ? std::max(0.f, applyDeadzone(gamepadState_.axes[glfwAxis])) : 0.f;
    };
    const auto axisNegative = [&](int glfwAxis) {
        return hasGamepad_ ? std::max(0.f, -applyDeadzone(gamepadState_.axes[glfwAxis])) : 0.f;
    };

    switch (action) {
        case GameAction::MoveForward:
            return keyDown(GLFW_KEY_W) ? 1.f : axisNegative(GLFW_GAMEPAD_AXIS_LEFT_Y);
        case GameAction::MoveBack:
            return keyDown(GLFW_KEY_S) ? 1.f : axisPositive(GLFW_GAMEPAD_AXIS_LEFT_Y);
        case GameAction::MoveLeft:
            return keyDown(GLFW_KEY_A) ? 1.f : axisNegative(GLFW_GAMEPAD_AXIS_LEFT_X);
        case GameAction::MoveRight:
            return keyDown(GLFW_KEY_D) ? 1.f : axisPositive(GLFW_GAMEPAD_AXIS_LEFT_X);
        case GameAction::Jump:
        case GameAction::Run:
            return isPressed(action) ? 1.f : 0.f;
        case GameAction::CamRight:
            return axisPositive(GLFW_GAMEPAD_AXIS_RIGHT_X);
        case GameAction::CamLeft:
            return axisNegative(GLFW_GAMEPAD_AXIS_RIGHT_X);
        case GameAction::CamUp:
            return axisNegative(GLFW_GAMEPAD_AXIS_RIGHT_Y);
        case GameAction::CamDown:
            return axisPositive(GLFW_GAMEPAD_AXIS_RIGHT_Y);
    }
    return 0.f;
}

bool InputManager::isPressed(GameAction action) const
{
    const int8_t injected = injected_[static_cast<size_t>(action)];
    if (injected >= 0) return injected != 0;

    const auto keyDown = [this](int key) {
        return window_ && glfwGetKey(window_, key) == GLFW_PRESS;
    };
    const auto btnDown = [this](int button) {
        return hasGamepad_ && gamepadState_.buttons[button] == GLFW_PRESS;
    };

    switch (action) {
        case GameAction::Jump: return keyDown(GLFW_KEY_SPACE)       || btnDown(GLFW_GAMEPAD_BUTTON_A);
        case GameAction::Run:  return keyDown(GLFW_KEY_LEFT_SHIFT)  || btnDown(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER);
        default: return getAxis(action) > 0.5f; // movement/camera actions: derive from analog magnitude
    }
}

void InputManager::onMouseButton(int button, int action, int /*mods*/)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        rightDown_ = (action == GLFW_PRESS);
}

void InputManager::onCursorPos(double x, double y)
{
    const glm::vec2 pos{ static_cast<float>(x), static_cast<float>(y) };
    if (firstCursorSample_) {
        lastCursor_        = pos;
        firstCursorSample_ = false;
    }

    if (rightDown_)
        accumMouseDelta_ += (pos - lastCursor_);
    lastCursor_ = pos;
}

} // namespace Phantom::Input
