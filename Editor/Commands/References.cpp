#include "References.h"

#include <algorithm>

#include <climits>
#include <cstdlib>

namespace Phantom::Editor::CommandCore {

bool globMatch(const std::string& pattern, const std::string& name) {
    size_t p = 0, n = 0, star = std::string::npos, match = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) { ++p; ++n; }
        else if (p < pattern.size() && pattern[p] == '*') { star = p++; match = n; }
        else if (star != std::string::npos) { p = star + 1; n = ++match; }
        else return false;
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

bool parseIdReference(const std::string& ref, int& id) {
    if (ref.empty() || ref[0] != '@') return false;
    char* end = nullptr;
    const long value = std::strtol(ref.c_str() + 1, &end, 10);
    if (end == ref.c_str() + 1 || *end != '\0' || value < 0 || value > INT_MAX) return false;
    id = static_cast<int>(value);
    return true;
}

void appendUnique(std::vector<int>& out, int id) {
    if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
}

} // namespace Phantom::Editor::CommandCore
