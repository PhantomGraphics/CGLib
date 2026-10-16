#include "AssetUri.h"

#include <vector>

namespace Phantom::Asset {

namespace {

std::vector<std::string> splitSegments(const std::string& normalized)
{
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start <= normalized.size()) {
        const std::size_t slash = normalized.find('/', start);
        const std::size_t end = (slash == std::string::npos) ? normalized.size() : slash;
        segments.push_back(normalized.substr(start, end - start));
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return segments;
}

} // namespace

std::optional<AssetUri> AssetUri::parse(const std::string& raw)
{
    if (raw.empty()) return std::nullopt;

    std::string normalized;
    normalized.reserve(raw.size());
    for (char c : raw) normalized += (c == '\\') ? '/' : c;

    if (normalized.front() == '/') return std::nullopt;              // absolute (POSIX-style)
    if (normalized.size() >= 2 && normalized[1] == ':') return std::nullopt; // "C:..." drive letter
    if (normalized.front() == '~') return std::nullopt;              // home-relative

    const std::vector<std::string> segments = splitSegments(normalized);
    if (segments.empty()) return std::nullopt;
    for (const std::string& seg : segments) {
        if (seg.empty() || seg == "." || seg == "..") return std::nullopt;
    }

    std::string rebuilt;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i) rebuilt += '/';
        rebuilt += segments[i];
    }
    return AssetUri(std::move(rebuilt));
}

std::filesystem::path AssetUri::toPath(const std::filesystem::path& projectRoot) const
{
    std::filesystem::path path = projectRoot;
    for (const std::string& seg : splitSegments(value_)) path /= seg;
    return path;
}

} // namespace Phantom::Asset
