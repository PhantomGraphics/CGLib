#pragma once

#include <string>
#include <vector>

namespace Phantom::Editor::CommandCore {
// Shell-style match with '*' and '?'.
bool globMatch(const std::string& pattern, const std::string& name);
// Parses a non-negative integer ID reference such as "@12".
bool parseIdReference(const std::string& ref, int& id);
void appendUnique(std::vector<int>& out, int id);
} // namespace Phantom::Editor::CommandCore
