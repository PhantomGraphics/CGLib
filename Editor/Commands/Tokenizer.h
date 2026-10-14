#pragma once

#include <string>
#include <vector>

namespace Phantom::Editor {

struct ParsedCommand {
    std::vector<std::string> tokens;
    std::vector<bool> quotedAtStart;
};

// Returns false for malformed quotes/escapes. # starts a comment at a token
// boundary, while color values such as color=#ff8800 remain ordinary tokens.
bool tokenize(const std::string& line, ParsedCommand& out, std::string& error);

// Quotes a token so that tokenize() reads it back unchanged (used when saving scripts and keymaps).
std::string quoteToken(const std::string& token);

std::string toLower(std::string s);

} // namespace Phantom::Editor
