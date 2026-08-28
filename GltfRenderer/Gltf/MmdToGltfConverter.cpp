#include "MmdToGltfConverter.h"
#include "MmdAnimationBaker.h"
#include "SkeletonGltfConverter.h"
#include "../Renderer/CameraUBO.h" // kMaxGltfBones

#include "../../File/File/PMXFileReader.h"
#include "../../File/File/VMDFileReader.h"
#include "../../Animation/Animation/PMXConverter.h"
#include "../../Animation/Animation/VMDConverter.h"

#include <cstdio>
#include <map>

using namespace Phantom::Gltf;
using namespace Phantom::Animation;

bool MmdToGltfConverter::convert(const std::filesystem::path& pmxPath,
                                  const std::filesystem::path& vmdPath,
                                  GltfDocument& out,
                                  const MmdToGltfOptions& options,
                                  MmdToGltfLoadStats* outStats)
{
    Phantom::File::PMXFileReader pmxReader;
    if (!pmxReader.read(pmxPath)) {
        std::fprintf(stderr, "[MmdToGltfConverter] PMX read failed: %s\n", pmxPath.string().c_str());
        return false;
    }
    const auto& pmx = pmxReader.getPMX();

    PMXConverter conv;
    Skeleton     skeleton;
    SkinnedMesh  mesh;
    if (!conv.convert(pmx, skeleton, mesh)) {
        std::fprintf(stderr, "[MmdToGltfConverter] PMX convert failed: %s\n", pmxPath.string().c_str());
        return false;
    }

    if (skeleton.bones.size() > kMaxGltfBones) {
        std::fprintf(stderr, "[MmdToGltfConverter] skeleton has %zu bones, exceeds kMaxGltfBones (%u): %s\n",
                     skeleton.bones.size(), kMaxGltfBones, pmxPath.string().c_str());
        return false;
    }

    std::vector<IKChain> ikChains;
    conv.convertIK(pmx, ikChains);

    std::vector<MorphTarget> morphTargets;
    conv.convertMorphs(pmx, morphTargets);

    std::vector<MmdSubMesh> subMeshes;
    conv.convertMaterials(pmx, subMeshes);

    AnimationClip bakedClip; // stays empty (== bind pose) if there's no VMD or it fails to load
    std::vector<std::pair<float, std::vector<float>>> bakedMorphWeights;

    if (!vmdPath.empty()) {
        Phantom::File::VMDFileReader vmdReader;
        if (vmdReader.read(vmdPath)) {
            const auto& vmd = vmdReader.getVMD();

            VMDConverter  vconv;
            AnimationClip rawClip;
            if (vconv.convert(vmd, skeleton.boneNameToIndex, rawClip)) {
                bakedClip = MmdAnimationBaker::bakeIk(skeleton, ikChains, rawClip, options.ikBakeSampleRateHz);
            } else {
                std::fprintf(stderr, "[MmdToGltfConverter] VMD bone convert failed, using bind pose: %s\n",
                             vmdPath.string().c_str());
            }

            if (!morphTargets.empty()) {
                std::map<std::string, int> morphNameToIndex;
                for (size_t i = 0; i < morphTargets.size(); ++i)
                    morphNameToIndex[morphTargets[i].name] = static_cast<int>(i);

                MorphAnimationClip morphClip;
                if (vconv.convertMorphs(vmd, morphNameToIndex, morphClip))
                    bakedMorphWeights = MmdAnimationBaker::bakeMorphWeights(morphTargets, morphClip);
            }
        } else {
            std::fprintf(stderr, "[MmdToGltfConverter] VMD read failed, using bind pose: %s\n", vmdPath.string().c_str());
        }
    }

    const std::string modelDir = pmxPath.parent_path().string();

    SkeletonGltfConverter gltfConv;
    if (!gltfConv.convert(skeleton, mesh, subMeshes, pmx.textures, modelDir,
                           bakedClip, morphTargets, bakedMorphWeights, out)) {
        std::fprintf(stderr, "[MmdToGltfConverter] Skeleton->glTF conversion failed: %s\n", pmxPath.string().c_str());
        return false;
    }

    if (outStats) {
        outStats->boneCount       = static_cast<int>(skeleton.bones.size());
        outStats->ikChainCount    = static_cast<int>(ikChains.size());
        outStats->morphTargetCount = static_cast<int>(morphTargets.size());
        outStats->vertexCount     = static_cast<int>(mesh.vertices.size());
    }

    return true;
}
