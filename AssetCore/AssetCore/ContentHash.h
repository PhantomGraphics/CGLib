#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace Phantom::Asset {

// A portable SHA-256 content hash (docs/spec/phantom_import_diagnostic.md /
// phantom_bridge's bridge_core._sha256()). The file helper deliberately lives in AssetCore
// rather than a viewer so every consumer can use the same cache key and verify a manifest.
class ContentHash {
public:
    ContentHash() = default;
    explicit ContentHash(std::string value) : value_(std::move(value)) {}

    // Returns nullopt when the file cannot be opened/read. The input is streamed in chunks,
    // so large GLB files do not need to be loaded into memory just to form a cache key.
    static std::optional<ContentHash> fromFile(const std::filesystem::path& path);
    static ContentHash fromBytes(const void* data, std::size_t size);

    bool isValid() const { return !value_.empty(); }
    const std::string& value() const { return value_; }

    bool operator==(const ContentHash& other) const { return value_ == other.value_; }
    bool operator!=(const ContentHash& other) const { return !(*this == other); }

private:
    std::string value_;
};

} // namespace Phantom::Asset
