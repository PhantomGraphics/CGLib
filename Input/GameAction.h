#pragma once

namespace Phantom::Input {

enum class GameAction {
    // Character movement
    MoveForward,
    MoveBack,
    MoveLeft,
    MoveRight,
    Jump,
    Run,
    // Camera (gamepad right stick only -- desktop camera orbit uses the mouse instead,
    // see InputManager::getMouseDelta()/isRightDragging())
    CamRight,
    CamLeft,
    CamUp,
    CamDown,
};

} // namespace Phantom::Input
