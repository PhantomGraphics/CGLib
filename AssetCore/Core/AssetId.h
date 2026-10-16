#pragma once

#include <functional>
#include <string>

namespace Phantom::Asset {

// A stable, project-scoped identifier for an asset or entity (Blender->Universe
// authoring loop Phase 2 item 1/4 -- docs/spec/phantom_asset_contract.md Sec.8). Wraps
// whatever string the Blender side minted as `phantom_uuid` (a uuid4 string,
// CGApp/blender/phantom_bridge/bridge_core.py's assign_uuids()), or any other string a
// caller chooses for assets with no Blender origin. Phantom never parses or validates
// the contents (no uuid version/variant checks) -- the two producers (phantom_bridge /
// Phantom importers) only need to agree it is opaque and stays stable across rename and
// reorder (and is reminted only on an actual duplicate collision).
class AssetId {
public:
    AssetId() = default;
    explicit AssetId(std::string value) : value_(std::move(value)) {}

    // Mints a fresh, random UUIDv4-formatted id (8-4-4-4-12 lowercase hex, RFC 4122
    // version/variant bits set) -- for callers that need a new stable id and have no
    // Blender-authored `phantom_uuid` to adopt, e.g. migrating legacy data that predates
    // the UUID contract (docs/spec/phantom_asset_contract.md Sec.8).
    static AssetId generate();

    bool isValid() const { return !value_.empty(); }
    const std::string& value() const { return value_; }

    bool operator==(const AssetId& other) const { return value_ == other.value_; }
    bool operator!=(const AssetId& other) const { return !(*this == other); }
    bool operator<(const AssetId& other) const { return value_ < other.value_; } // ordered containers

private:
    std::string value_;
};

} // namespace Phantom::Asset

namespace std {
template <>
struct hash<Phantom::Asset::AssetId> {
    size_t operator()(const Phantom::Asset::AssetId& id) const noexcept {
        return std::hash<std::string>{}(id.value());
    }
};
} // namespace std
