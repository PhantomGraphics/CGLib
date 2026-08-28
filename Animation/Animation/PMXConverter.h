#pragma once
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "CGLib/File/File/PMXFile.h"
#include "IKSolver.h"
#include "MmdMaterial.h"
#include "MorphTarget.h"
#include "Skeleton.h"
#include "SkinnedMesh.h"

namespace Phantom::Animation {

// Converts a PMXFile (raw binary) to Skeleton + SkinnedMesh + IKChains + MorphTargets.
// Handles MMD left-hand Z-forward → right-hand Z-backward conversion.
// Names are already UTF-8 (PMX 2.0 native) — no Shift-JIS conversion needed.
class PMXConverter {
public:
    bool convert(const Phantom::File::PMXFile& pmx,
                 Skeleton& outSkeleton,
                 SkinnedMesh& outMesh);

    // Convert PMX IK bones to IKChain list.
    void convertIK(const Phantom::File::PMXFile& pmx,
                   std::vector<IKChain>& outChains);

    // Convert PMX vertex morphs (morphType == 1) to MorphTarget list.
    // System morphs (category == 0) are excluded.
    bool convertMorphs(const Phantom::File::PMXFile& pmx,
                       std::vector<MorphTarget>& outTargets);

    // Convert PMX materials to MmdSubMesh list for multi-material rendering.
    bool convertMaterials(const Phantom::File::PMXFile& pmx,
                          std::vector<MmdSubMesh>& outSubMeshes);

private:
    static glm::vec3 convertPos(float x, float y, float z) { return {x, y, -z}; }
};

} // namespace Phantom::Animation
