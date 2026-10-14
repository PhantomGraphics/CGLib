#include "Tokenizer.h"

#include <algorithm>
#include <cctype>

namespace Phantom::Editor {

bool tokenize(const std::string& line, ParsedCommand& out, std::string& error) {
    out.tokens.clear();
    out.quotedAtStart.clear();
    error.clear();
    std::string token;
    bool quoted = false, active = false, quotedAtStart = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"') { quoted = false; continue; }
            if (c == '\\') {
                if (i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\'))
                    token.push_back(line[++i]);
                else
                    token.push_back(c); // Windows paths use literal backslashes.
                continue;
            }
            token.push_back(c);
        } else if (c == '"') {
            if (!active) quotedAtStart = true;
            quoted = true;
            active = true;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            if (active) {
                out.tokens.push_back(token);
                out.quotedAtStart.push_back(quotedAtStart);
                token.clear(); active = false; quotedAtStart = false;
            }
        } else if (c == '#' && !active) {
            break;
        } else {
            token.push_back(c);
            active = true;
        }
    }
    if (quoted) { error = "unterminated quote"; return false; }
    if (active) { out.tokens.push_back(token); out.quotedAtStart.push_back(quotedAtStart); }
    return true;
}

std::string quoteToken(const std::string& token) {
    std::string out = "\"";
    for (char c : token) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace Phantom::Editor
