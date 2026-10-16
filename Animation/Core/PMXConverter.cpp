#include "PMXConverter.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cstring>

namespace Phantom::Animation {

bool PMXConverter::convert(const Phantom::File::PMXFile& pmx,
                            Skeleton& outSkeleton,
                            SkinnedMesh& outMesh)
{
    outSkeleton = Skeleton{};
    outMesh     = SkinnedMesh{};

    const int n = static_cast<int>(pmx.bones.size());

    // Step 1: collect world-space bind positions and register bones
    std::vector<glm::vec3> worldPos(n);
    for (int i = 0; i < n; ++i) {
        const auto& pb = pmx.bones[i];
        Bone bone;
        bone.name          = pb.name.empty() ? ("Bone_" + std::to_string(i)) : pb.name;
        bone.parentIndex   = pb.parentBoneIndex; // PMX already uses -1 for root
        bone.localPosition = convertPos(pb.position[0], pb.position[1], pb.position[2]);
        bone.localRotation = glm::quat{1.f, 0.f, 0.f, 0.f};
        bone.localScale    = glm::vec3{1.f};
        worldPos[i]        = bone.localPosition;
        outSkeleton.addBone(bone);
    }

    // Step 2: bindPoseInverse from world positions
    for (int i = 0; i < n; ++i) {
        outSkeleton.bones[i].bindPoseInverse =
            glm::inverse(glm::translate(glm::mat4{1.f}, worldPos[i]));
    }

    // Step 3: convert localPosition to parent-relative offset
    for (int i = 0; i < n; ++i) {
        const int p = outSkeleton.bones[i].parentIndex;
        if (p >= 0 && p < n)
            outSkeleton.bones[i].localPosition = worldPos[i] - worldPos[p];
    }

    // Vertices: BDEF1/2/4 and SDEF are already normalised in PMXVertex
    outMesh.vertices.reserve(pmx.vertices.size());
    for (const auto& pv : pmx.vertices) {
        SkinVertex sv{};
        sv.position = convertPos(pv.position[0], pv.position[1], pv.position[2]);
        sv.normal   = convertPos(pv.normal[0],   pv.normal[1],   pv.normal[2]);
        sv.texCoord = {pv.uv[0], pv.uv[1]};
        sv.color    = {0.8f, 0.8f, 0.8f, 1.f};

        // Normalise BDEF4 weights (spec says sum == 1, but guard anyway)
        float w[4] = {pv.boneWeights[0], pv.boneWeights[1],
                      pv.boneWeights[2], pv.boneWeights[3]};
        const float wSum = w[0] + w[1] + w[2] + w[3];
        if (wSum > 1e-6f) {
            for (float& f : w) f /= wSum;
        } else {
            w[0] = 1.f; w[1] = w[2] = w[3] = 0.f;
        }
        sv.boneWeights = {w[0], w[1], w[2], w[3]};

        // Clamp negative (unused) bone indices to 0 — weight is 0 so no effect
        for (int k = 0; k < 4; ++k)
            sv.boneIndices[k] = (pv.boneIndices[k] >= 0) ? pv.boneIndices[k] : 0;

        outMesh.vertices.push_back(sv);
    }

    outMesh.indices.reserve(pmx.indices.size());
    for (int32_t idx : pmx.indices)
        outMesh.indices.push_back(static_cast<uint32_t>(idx));

    return true;
}

void PMXConverter::convertIK(const Phantom::File::PMXFile& pmx,
                               std::vector<IKChain>& outChains)
{
    outChains.clear();
    const int n = static_cast<int>(pmx.bones.size());
    for (int i = 0; i < n; ++i) {
        const auto& b = pmx.bones[i];
        if (!(b.flags & 0x0020)) continue;

        IKChain chain;
        chain.effectorBoneIndex = b.ikTargetBoneIndex;
        chain.targetBoneIndex   = i;
        chain.iterationCount    = b.ikLoopCount;
        chain.angleLimitRad     = glm::clamp(b.ikAngleLimit, 1e-3f, glm::pi<float>());
        chain.chainBones.reserve(b.ikLinks.size());
        for (const auto& link : b.ikLinks)
            chain.chainBones.push_back(link.boneIndex);
        outChains.push_back(std::move(chain));
    }
}

bool PMXConverter::convertMorphs(const Phantom::File::PMXFile& pmx,
                                   std::vector<MorphTarget>& outTargets)
{
    outTargets.clear();
    for (const auto& morph : pmx.morphs) {
        if (morph.morphType != 1) continue; // vertex morphs only
        if (morph.category == 0)  continue; // skip system / hidden

        MorphTarget target;
        target.name = morph.name.empty()
                      ? ("Morph_" + std::to_string(outTargets.size()))
                      : morph.name;

        target.deltas.reserve(morph.vertexMorphs.size());
        for (const auto& vm : morph.vertexMorphs) {
            MorphVertex delta;
            delta.vertexIndex    = vm.vertexIndex;
            delta.positionOffset = convertPos(vm.offset[0], vm.offset[1], vm.offset[2]);
            target.deltas.push_back(delta);
        }
        outTargets.push_back(std::move(target));
    }
    return true;
}

bool PMXConverter::convertMaterials(const Phantom::File::PMXFile& pmx,
                                     std::vector<MmdSubMesh>& outSubMeshes)
{
    outSubMeshes.clear();
    uint32_t offset = 0;
    for (const auto& m : pmx.materials) {
        MmdSubMesh sub{};
        sub.indexOffset      = offset;
        sub.indexCount       = static_cast<uint32_t>(m.faceCount);
        sub.textureIdx       = m.textureIndex;
        sub.sphereTextureIdx = m.sphereTextureIndex;
        sub.toonTextureIdx   = m.toonTextureIndex;
        sub.props.diffuse    = { m.diffuse[0], m.diffuse[1], m.diffuse[2], m.diffuse[3] };
        sub.props.specular   = { m.specular[0], m.specular[1], m.specular[2] };
        sub.props.specularity= m.specularity;
        sub.props.ambient    = { m.ambient[0], m.ambient[1], m.ambient[2] };
        sub.props.drawFlags  = m.drawFlags;
        sub.props.edgeColor  = { m.edgeColor[0], m.edgeColor[1], m.edgeColor[2], m.edgeColor[3] };
        sub.props.edgeSize   = m.edgeSize;
        sub.props.sphereMode = m.sphereMode;
        sub.props.toonShared = (m.toonSharingFlag == 1);
        offset += static_cast<uint32_t>(m.faceCount);
        outSubMeshes.push_back(std::move(sub));
    }
    return true;
}

} // namespace Phantom::Animation
