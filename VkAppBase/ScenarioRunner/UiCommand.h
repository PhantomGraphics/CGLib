#pragma once
// GUI-originated commands share a dispatcher's input queue with typed and
// scenario commands (so everything runs in order on the render thread), but
// nothing reads their response. They are marked with a leading control
// character that processQueue() strips before routing; the response is dropped.
//
//   submitUi(cmd):  dispatch(markUiCommand(cmd))       // in the dispatcher
//   processQueue(): bool ui = takeUiMark(cmd); resp = route(cmd); if (!ui) push(resp);
#include <string>

inline std::string markUiCommand(const std::string& cmd) { return std::string(1, '\x01') + cmd; }

// Strips the mark. Returns true when `cmd` was a GUI command.
inline bool takeUiMark(std::string& cmd) {
    if (cmd.empty() || cmd[0] != '\x01') return false;
    cmd.erase(0, 1);
    return true;
}
