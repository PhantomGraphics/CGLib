#pragma once

#include "CGLib/AssetCore/AssetCore/AssetManifest.h"

#include "SceneGraph.h"

#include <string>
#include <vector>

namespace Phantom::SceneRuntime::Universe {

// A ".universe" v2 document (Blender->Universe authoring loop Phase 2 item 3,
// docs/todo/PLAN_blender_universe_authoring_loop.md Sec.5.2): a project-relative asset
// manifest plus a UUID node hierarchy, replacing v1's flat, unidentified entity list. Built
// entirely out of the existing Phantom::Asset::AssetManifest and Phantom::SceneRuntime::
// SceneGraph rather than a parallel bespoke format -- an entity that instances a shared
// asset (glTF today) carries that reference as an ordinary "meshAsset" component
// ({"assetId":..., "nodeId":...}) rather than a dedicated struct field, keeping the entity
// shape identical for asset-backed and asset-free (primitive/embedded mesh, rigidBody,
// cloth, fluid, ...) nodes alike.
//
// Not implemented yet: `physics`/`renderSettings` stay opaque JSON passthrough (no typed
// schema), and nothing in Universe (CGApp, MSBuild-built) reads or writes this format --
// migrateV1ToV2() below only produces the v2 *document*; wiring Universe's own
// UniverseSceneIO.cpp to actually load/save it is deferred to Phase 2 item 5 (replacing the
// one-GltfSceneRenderer-per-entity model with the shared-asset instancing v2 assumes).
struct SceneV2 {
    static constexpr int kVersion = 2;

    Phantom::Asset::AssetManifest assets;
    // Optional path from the .universe file's directory to the project asset root.
    // Empty preserves legacy caller-relative URI resolution. Never an absolute path.
    std::string assetRoot;
    SceneGraph scene;
    nlohmann::json physics = nlohmann::json::object();
    nlohmann::json renderSettings = nlohmann::json::object();

    std::string toJson() const;
    static SceneV2 fromJson(const std::string& json, bool* ok = nullptr);

    // Deep copy for edit/play mode switching -- the remaining piece of Phase 2 item 2's
    // "edit snapshotとplay snapshot" (Phase 5's own completion criteria: "Play開始時にscene
    // snapshotを作り、Stopで編集状態へ戻す"). SceneV2 already holds no pointers (AssetManifest/
    // SceneGraph are plain value types, nlohmann::json copies deep), so plain copy
    // construction/assignment already does this correctly; snapshot()/restoreFrom() add
    // nothing beyond that -- they exist so callers have a named, tested contract to rely on
    // instead of each independently re-confirming "does copying this actually deep-copy".
    SceneV2 snapshot() const { return *this; }
    void restoreFrom(const SceneV2& snapshot) { *this = snapshot; }
};

struct MigrationResult {
    bool ok = false;
    SceneV2 scene;
    // Non-fatal, per-entity notes (e.g. a mesh path that wasn't a valid project-relative
    // URI) -- migrateV1ToV2() never silently drops a problem, per Sec.5.2's "読み込み時に
    // 黙ってcomponentを落とさず、partial load結果とdiagnosticを返す" principle. Empty when
    // `ok` is false too (in that case the whole document was rejected, see below).
    std::vector<std::string> diagnostics;
};

// Reads a legacy ".universe" v1 document (CGApp/Universe/UniverseSceneIO.cpp: "version":1,
// a flat `entities` array of name/visible/transform/mesh/rigidBody/cloth/fluid/volume blocks, no
// persisted entity id -- Universe's own `nextId_` counter stands in for identity today) and
// produces a v2 document. Every entity gets a freshly minted NodeId (AssetId::generate() --
// v1 never had one to carry forward). A "gltf" mesh reference becomes a new AssetManifest
// entry plus a "meshAsset" component ({"assetId", "nodeId": "" -- node-level referencing
// within a shared asset is Phase 2 item 5's job, not populated here}); "primitive"/
// "embedded" meshes have no backing asset file to reference (embedded literally has none by
// design, CGApp/Universe/CLAUDE.md's ".universe シーンファイル形式" section) and stay
// inline as a "mesh" component instead; rigidBody/cloth/fluid/volume fold into same-named
// components verbatim (no typed schema conversion attempted, consistent with the rest of
// item 2/3's first slices).
//
// Malformed input (JSON parse error, wrong/missing "version", missing "entities") fails the
// whole document (`ok=false`, empty `scene`). A per-entity problem instead degrades that one
// entity (its mesh reference is dropped, everything else about it -- transform, other
// components -- still migrates) and is recorded in `diagnostics`, so an entity is never
// dropped from the document as a whole over one bad field.
MigrationResult migrateV1ToV2(const std::string& v1Json);

} // namespace Phantom::SceneRuntime::Universe
