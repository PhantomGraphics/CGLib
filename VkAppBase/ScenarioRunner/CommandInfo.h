#pragma once
#include <string>

// One entry of an app's command catalog (help / completion metadata).
// `name` must be a command the app's dispatcher really routes; `args` is a
// short usage hint ("0|1", "theta,phi,distance", empty for no argument).
struct CommandInfo {
    std::string name;
    std::string args;
    std::string help;
};
