#pragma once

#include <string>
#include <vector>

namespace Phantom::Editor {

// Reads a .phs script (also used for .ukeymap): one command per line. Every line is returned,
// including blank and comment lines, so that index + 1 is the line number to report in errors.
// Strips a UTF-8 BOM and trailing CR. Returns false when the file cannot be read.
bool readScriptLines(const std::string& path, std::vector<std::string>& lines);

// Same, from text already in memory.
std::vector<std::string> splitScriptLines(const std::string& text);

} // namespace Phantom::Editor
