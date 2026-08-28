#pragma once

#include "GltfDocument.h"

#include <filesystem>

namespace Phantom::Gltf {

struct MmdToGltfOptions {
    float ikBakeSampleRateHz = 30.f; // forwarded as-is to MmdAnimationBaker::bakeIk()
};

// Source-data counts from the PMX/VMD conversion that don't survive into the output
// GltfDocument in a directly-recoverable form -- most notably ikChainCount, since IK chains are
// fully consumed by MmdAnimationBaker::bakeIk() and leave no trace distinguishing a baked IK
// bone's channel from any other dense bone channel. boneCount/morphTargetCount/vertexCount are
// technically derivable from the document too (skins[0].joints.size(), meshes[0].primitives[0]
// .targets.size(), an accessor's count), but are surfaced here as well since callers that just
// want a load-time summary (e.g. AnimationView's scenario/UI stats) shouldn't have to re-derive
// them from raw glTF accessors.
struct MmdToGltfLoadStats {
    int boneCount        = 0;
    int ikChainCount      = 0;
    int morphTargetCount  = 0;
    int vertexCount       = 0;
};

// Single entry point from PMX (+ optional VMD) files straight to a complete GltfDocument --
// shape, textures, skin, baked bone animation, morph targets, and baked morph-weight animation
// all in one document (mirrors ObjToGltfConverter::convert()/StlToGltfConverter::convert(), just
// with a PMX+VMD pair instead of a single input file -- see
// docs/todo/PLAN_mmd_gltf_unification.md Phase 6).
class MmdToGltfConverter {
public:
    // vmdPath empty (or unreadable) -> no animation/morph-weight channels, bind pose only (same
    // fallback Phantom::Animation::Animator::computeFK() already applies to bones with no
    // channel). Texture paths are resolved automatically from PMXFile::textures +
    // pmxPath.parent_path(). Returns false if the PMX fails to load/convert, or if its skeleton
    // exceeds kMaxGltfBones (256, see Renderer/CameraUBO.h) -- GPU skinning can't address more
    // joints than that (this check is the one MmdCharacterModel::load() used to do itself).
    // outStats, if non-null, is filled in whenever convert() succeeds (left untouched on failure).
    static bool convert(const std::filesystem::path& pmxPath,
                         const std::filesystem::path& vmdPath,
                         GltfDocument& out,
                         const MmdToGltfOptions& options = {},
                         MmdToGltfLoadStats* outStats = nullptr);
};

} // namespace Phantom::Gltf
