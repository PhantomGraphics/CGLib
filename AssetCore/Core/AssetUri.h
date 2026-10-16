#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace Phantom::Asset {

// A project-relative asset URI: forward-slash separated, UTF-8, never absolute, never
// escapes the project root via ".." or a drive letter (docs/spec/phantom_asset_contract.md
// Sec.7 -- the only path shape allowed to appear in a manifest, .universe entity, or
// Compatibility Report). Backslashes are normalized to forward slashes on parse (a
// project-relative path is a portable identifier, not a native OS path -- the native
// path only exists after toPath() resolves it against a concrete project root).
class AssetUri {
public:
    // Default-constructed = invalid/empty (so AssetManifestEntry, which embeds one, stays
    // default-constructible); use parse() to get a real one.
    AssetUri() = default;

    // Validates and normalizes `raw`. Returns std::nullopt if `raw` is empty, absolute
    // (leading '/'), a Windows drive-letter path ("C:..."), home-relative ("~..."), or
    // contains a "." or ".." segment -- asset URIs identify a file, they do not navigate,
    // so even a harmless-looking ".." is rejected rather than silently resolved.
    static std::optional<AssetUri> parse(const std::string& raw);

    bool isValid() const { return !value_.empty(); }
    const std::string& value() const { return value_; }
    bool operator==(const AssetUri& other) const { return value_ == other.value_; }
    bool operator!=(const AssetUri& other) const { return !(*this == other); }

    // Resolves this URI against `projectRoot` into a native filesystem path.
    std::filesystem::path toPath(const std::filesystem::path& projectRoot) const;

private:
    explicit AssetUri(std::string value) : value_(std::move(value)) {}
    std::string value_;
};

} // namespace Phantom::Asset
