#include "Script.h"

#include <fstream>
#include <iterator>

namespace Phantom::Editor {

std::vector<std::string> splitScriptLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) start = 3;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // A trailing newline does not start another (empty) line.
        if (end == text.size() && line.empty()) break;
        lines.push_back(std::move(line));
        start = end + 1;
    }
    return lines;
}

bool readScriptLines(const std::string& path, std::vector<std::string>& lines) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    lines = splitScriptLines(text);
    return true;
}

} // namespace Phantom::Editor
