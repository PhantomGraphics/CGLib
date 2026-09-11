#pragma once

#include "AssetId.h"
#include "AssetUri.h"
#include "ContentHash.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::Asset {

// One asset's manifest record (docs/spec/phantom_asset_manifest.md, schema
// "phantom.manifest/1"). `dependencies` is carried but never populated yet -- the
// dependency-graph half of Phase 2 item 1 is still unimplemented.
struct AssetManifestEntry {
    AssetId id;
    AssetUri uri;
    ContentHash contentHash;
    std::string sourceBlend;           // project-relative when known; may be "" (Phase 1 note, contract Sec.7)
    std::vector<AssetId> dependencies;
};

// In-memory project asset manifest: AssetId -> {uri, contentHash, dependencies}
// (Blender->Universe authoring loop Phase 2 item 1). This is the foundation only -- the
// file watcher / debounce / atomic publish / diagnostic store that turn a manifest into a
// live hot-reload system are Phase 2 items 6/7, not implemented here.
class AssetManifest {
public:
    void upsert(AssetManifestEntry entry);
    bool remove(const AssetId& id);
    const AssetManifestEntry* find(const AssetId& id) const;
    std::size_t size() const { return entries_.size(); }
    const std::unordered_map<std::string, AssetManifestEntry>& entries() const { return entries_; }

    // Single-line "phantom.manifest/1" JSON.
    std::string toJson() const;
    static AssetManifest fromJson(const std::string& json, bool* ok = nullptr);

    bool saveToFile(const std::filesystem::path& path) const;
    static AssetManifest loadFromFile(const std::filesystem::path& path, bool* ok = nullptr);

private:
    std::unordered_map<std::string, AssetManifestEntry> entries_; // keyed by AssetId::value()
};

} // namespace Phantom::Asset
