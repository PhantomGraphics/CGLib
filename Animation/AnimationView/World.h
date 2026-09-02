#pragma once

#include "CGLib/GltfRenderer/Gltf/GltfDocument.h"

#include <cstdint>
#include <string>

namespace Phantom::Animation {

// Everything the app needs to play back a PMX(+VMD) model loaded via
// Phantom::Gltf::MmdToGltfConverter (see internal design notes Phase 8).
// Unlike the pre-migration version, this holds no Phantom::Animation types at all -- IK baking,
// skeleton/mesh conversion, and morph-weight baking all already happened once, at load time,
// inside MmdToGltfConverter::convert(). Per-frame pose comes from
// Phantom::Gltf::GltfAnimationEvaluator against `document` alone.
struct World {
    Phantom::Gltf::GltfDocument document;
    int animationIndex = -1; // -1 = document.animations is empty (bind pose only)
    int skinIndex       = 0;

    // --- Playback --------------------------------------------------------
    float currentTime = 0.f;
    float duration     = 0.f; // document.animations[animationIndex]'s length, 0 if none
    float speed        = 1.f;
    bool  playing       = false;
    bool  loop           = true;
    bool  dirty          = false;

    // --- Visibility --------------------------------------------------------
    bool showMesh  = true;
    // Bone-wire display was dropped in the glTF migration (Phase 8) along with BoneWireRenderer
    // -- there is no renderer left that consumes this flag. Kept only so
    // SetVisible:Bones:<0|1> stays a valid scenario command (existing scenarios exercise it).
    bool showBones = true;

    // --- IK ------------------------------------------------------------
    // Informational only: IK is already baked into document's bone animation channels by
    // MmdAnimationBaker::bakeIk() at load time (see MmdToGltfConverter), so toggling this no
    // longer changes the computed pose -- kept for scenario/UI compatibility
    // (SetIKEnabled/GetIKCount).
    bool ikEnabled = true;

    // --- Stats populated by AnimationViewApp after a successful load (Phantom::Gltf::
    // MmdToGltfLoadStats mirrors these -- see MmdToGltfConverter.h for why ikCount/morphCount
    // can't just be re-derived from `document` after baking) ---------------------------------
    int boneCount  = 0;
    int ikCount    = 0;
    int morphCount = 0;
    int vertCount  = 0;

    // --- Load requests (set by UI/dispatcher) ----------------------------
    std::string loadedModelPath;  // PMX file path
    std::string loadedMotionPath; // VMD file path

    // --- Load debug info (populated by AnimationViewApp after each load attempt) ---
    struct LoadDebugInfo {
        bool        attempted  = false;
        bool        success    = false;
        std::string filePath;
        std::string failedAt;    // PMX: section where parse failed
        int     vertCount  = -1;
        int     idxCount   = -1;
        int     texCount   = -1;
        int     matCount   = -1;
        int     boneCount  = -1;
        int     morphCount = -1;
        int64_t streamPosAtFail   = -1;
        int64_t boneSectionStart  = -1;
    } loadDebug;
};

} // namespace Phantom::Animation
