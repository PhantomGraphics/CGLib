#pragma once

#include <string>

namespace Phantom::Asset {

// An opaque "sha256:<hex>" content hash string (docs/spec/phantom_import_diagnostic.md /
// phantom_bridge's bridge_core._sha256()). This type only stores and compares a hash
// computed elsewhere; it does not implement SHA-256 itself -- no hashing algorithm lives
// in Phantom yet (the still-unimplemented rest of Phase 2 item 1, "import cache", needs
// one to skip re-parsing an unchanged asset; a manifest can carry the Blender-computed
// hash today without Phantom being able to recompute or verify it).
class ContentHash {
public:
    ContentHash() = default;
    explicit ContentHash(std::string value) : value_(std::move(value)) {}

    bool isValid() const { return !value_.empty(); }
    const std::string& value() const { return value_; }

    bool operator==(const ContentHash& other) const { return value_ == other.value_; }
    bool operator!=(const ContentHash& other) const { return !(*this == other); }

private:
    std::string value_;
};

} // namespace Phantom::Asset
