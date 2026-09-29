#include "InputManager.h"

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
}

float InputManager::getAxis(GameAction action) const
{
    const size_t i = static_cast<size_t>(action);
    if (injected_[i] >= 0) return injected_[i] != 0 ? 1.f : 0.f;
    return values_[i];
}

bool InputManager::isPressed(GameAction action) const
{
    return getAxis(action) > 0.5f;
}

bool InputManager::padButton(int glfwButton) const
{
    return hasGamepad_ && glfwButton >= 0 && glfwButton <= GLFW_GAMEPAD_BUTTON_LAST &&
           gamepadState_.buttons[glfwButton] == GLFW_PRESS;
}

float InputManager::padAxis(int glfwAxis) const
{
    if (!hasGamepad_ || glfwAxis < 0 || glfwAxis > GLFW_GAMEPAD_AXIS_LAST) return 0.f;
    return applyDeadzone(gamepadState_.axes[glfwAxis]);
}

} // namespace Phantom::Input
