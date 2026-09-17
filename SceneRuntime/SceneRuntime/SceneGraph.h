#pragma once

#include "SceneNode.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::SceneRuntime {

// Vulkan/ImGui-independent scene node hierarchy (Blender->Universe authoring loop Phase 2
// item 2, docs/spec/phantom_scene_runtime.md). First slice only: UUID/name/parent-children/
// local-world transform/enabled plus an untyped component store with JSON round-trip --
// the same "type definitions and file I/O only" scope AssetCore's own AssetManifest took
// for its first slice (docs/spec/phantom_asset_manifest.md). Not yet implemented, deferred
// to later increments of item 2: typed component schemas + migration, edit/play snapshots,
// and wiring into Universe/.universe v2 or the glTF importer (item 5).
class SceneGraph {
public:
    // Adds `node` as a new node. Fails (false, graph unchanged) if `node.id` is invalid,
    // already present, or `node.parent` is valid but not already in the graph -- parents
    // must be added before their children (the order Blender/glTF node arrays already
    // guarantee: a node's own index never appears in an earlier node's `children` list).
    bool addNode(SceneNode node);

    // Removes `id` and, recursively, every descendant. Returns false if `id` is absent.
    bool removeNode(const NodeId& id);

    // Reparents `id` under `newParent` (an invalid NodeId makes `id` a root). Fails if `id`
    // is absent, `newParent` is valid but absent, or `newParent` is `id` itself or one of
    // `id`'s own descendants (would create a cycle).
    bool reparent(const NodeId& id, const NodeId& newParent);

    const SceneNode* find(const NodeId& id) const;
    SceneNode* find(const NodeId& id);

    // Direct children only, in no particular order. An invalid `parent` returns the roots.
    // O(n) over every node -- deliberately not indexed yet (no caller needs it fast; add a
    // children-by-parent index alongside this if/when one does).
    std::vector<NodeId> children(const NodeId& parent) const;

    std::size_t size() const { return nodes_.size(); }

    // Composes local transforms from `id` up through its ancestors to its root. nullopt
    // only on a cycle or a stale parent reference -- addNode()/reparent() never create
    // either through this API, but a hand-edited or corrupted serialized graph could still
    // contain one, so this stays defensive rather than assuming callers only ever hand it
    // graphs this class itself built.
    std::optional<Phantom::Math::Matrix4df> worldTransform(const NodeId& id) const;

    // Single-line "phantom.scene/1" JSON (docs/spec/phantom_scene_runtime.md).
    std::string toJson() const;
    static SceneGraph fromJson(const std::string& json, bool* ok = nullptr);

    bool saveToFile(const std::filesystem::path& path) const;
    static SceneGraph loadFromFile(const std::filesystem::path& path, bool* ok = nullptr);

private:
    bool isSelfOrDescendant(const NodeId& start, const NodeId& candidate) const;

    std::unordered_map<std::string, SceneNode> nodes_; // keyed by NodeId::value()
};

} // namespace Phantom::SceneRuntime
