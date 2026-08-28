#pragma once

#include "GltfDocument.h"
#include "../../Animation/Animation/AnimationClip.h"
#include "../../Animation/Animation/MmdMaterial.h"
#include "../../Animation/Animation/MorphTarget.h"
#include "../../Animation/Animation/Skeleton.h"
#include "../../Animation/Animation/SkinnedMesh.h"

#include <string>
#include <utility>
#include <vector>

namespace Phantom::Gltf {

// Converts a generic Phantom::Animation skeletal mesh (Skeleton + SkinnedMesh) into a
// GltfDocument with real skin data (GltfSkin/JOINTS_0/WEIGHTS_0 -- see GltfTypes.h), so it can be
// rendered through GltfSceneRenderer's GPU skinning like any other glTF asset. PMX/VMD is just
// today's source for Skeleton/SkinnedMesh (via PMXConverter) -- this converter itself knows
// nothing about MMD beyond accepting MmdSubMesh for per-submesh materials.
//
// Only bind-pose geometry is built here. Per-frame pose comes from either
// Phantom::Animation::Animator::getSkinMatrices() (legacy, forwarded to
// GltfSceneRenderer::updateSkinMatrices() every frame) or, once an animation clip/morph weights
// are baked into the document below, GltfAnimationEvaluator -- this converter runs once, at load
// time either way.
class SkeletonGltfConverter {
public:
    // subMeshes empty -> a single primitive spanning all of mesh.indices, materialIndex=0, a
    // default untextured white material (mirrors GltfGpuMesh::build()'s own "absent = use a
    // harmless default" convention). texturePaths/modelDir are only consulted when subMeshes is
    // non-empty and a submesh references a texture (MmdSubMesh::textureIdx).
    //
    // MmdSubMesh::props.sphereMode/toonTextureIdx/edgeColor/edgeSize are intentionally dropped:
    // this targets GltfSceneRenderer's flat PBR pipeline, not MMD-style toon/sphere/edge shading.
    //
    // Static overload (no animation/morphs) -- forwards to the full overload below with empty
    // bakedClip/morphTargets/bakedMorphWeights, so the resulting document has no doc.animations
    // and no morph targets, identical to this converter's original behavior.
    bool convert(const Phantom::Animation::Skeleton& skeleton,
                 const Phantom::Animation::SkinnedMesh& mesh,
                 const std::vector<Phantom::Animation::MmdSubMesh>& subMeshes,
                 const std::vector<std::string>& texturePaths,
                 const std::string& modelDir,
                 GltfDocument& out);

    // Full overload: also builds bone animation channels from bakedClip (see
    // MmdAnimationBaker::bakeIk() -- IK-affected bones must already be densely baked, since
    // channels/samplers have no runtime IK-solving concept) and morph targets + a Weights
    // animation channel from morphTargets/bakedMorphWeights (see
    // MmdAnimationBaker::bakeMorphWeights()). Pass an empty AnimationClip{}/empty vectors for
    // whichever piece isn't needed -- e.g. a model with no VMD has bakedClip.channels empty.
    bool convert(const Phantom::Animation::Skeleton& skeleton,
                 const Phantom::Animation::SkinnedMesh& mesh,
                 const std::vector<Phantom::Animation::MmdSubMesh>& subMeshes,
                 const std::vector<std::string>& texturePaths,
                 const std::string& modelDir,
                 const Phantom::Animation::AnimationClip& bakedClip,
                 const std::vector<Phantom::Animation::MorphTarget>& morphTargets,
                 const std::vector<std::pair<float, std::vector<float>>>& bakedMorphWeights,
                 GltfDocument& out);
};

} // namespace Phantom::Gltf
