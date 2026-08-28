#include "SkeletonGltfConverter.h"
#include "GltfAccessorBuilder.h"

#include "../../../CGLib/ThirdParty/stb/stb_image.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Phantom::Gltf;
using namespace Phantom::Animation;

namespace {

bool decodeImageFile(GltfImage& img, const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::fprintf(stderr, "[SkeletonGltfConverter] cannot open texture: %s\n", path.string().c_str());
        return false;
    }
    const auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> raw(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(raw.data()), size);

    int w, h, ch;
    stbi_uc* px = stbi_load_from_memory(raw.data(), static_cast<int>(raw.size()), &w, &h, &ch, STBI_rgb_alpha);
    if (!px) {
        std::fprintf(stderr, "[SkeletonGltfConverter] stb_image failed to decode: %s\n", path.string().c_str());
        return false;
    }
    img.width    = w;
    img.height   = h;
    img.channels = 4;
    img.pixels.assign(px, px + w * h * 4);
    stbi_image_free(px);
    return true;
}

} // namespace

bool SkeletonGltfConverter::convert(const Skeleton& skeleton,
                                     const SkinnedMesh& mesh,
                                     const std::vector<MmdSubMesh>& subMeshes,
                                     const std::vector<std::string>& texturePaths,
                                     const std::string& modelDir,
                                     GltfDocument& out)
{
    return convert(skeleton, mesh, subMeshes, texturePaths, modelDir,
                    AnimationClip{}, {}, {}, out);
}

bool SkeletonGltfConverter::convert(const Skeleton& skeleton,
                                     const SkinnedMesh& mesh,
                                     const std::vector<MmdSubMesh>& subMeshes,
                                     const std::vector<std::string>& texturePaths,
                                     const std::string& modelDir,
                                     const AnimationClip& bakedClip,
                                     const std::vector<MorphTarget>& morphTargets,
                                     const std::vector<std::pair<float, std::vector<float>>>& bakedMorphWeights,
                                     GltfDocument& out)
{
    if (mesh.vertices.empty()) return false;

    out = GltfDocument{};

    // --- Joint node hierarchy: one GltfNode per bone. Spec-shaped and informational -- runtime
    // skinning is driven directly by Animator::getSkinMatrices() (via updateSkinMatrices()), not
    // by traversing these nodes, but keeping a real hierarchy here makes the document
    // self-describing and matches how a real animated glTF file would be shaped.
    const int boneCount = static_cast<int>(skeleton.bones.size());
    out.nodes.resize(boneCount);
    for (int i = 0; i < boneCount; ++i) {
        const auto& bone = skeleton.bones[i];
        GltfNode& node   = out.nodes[i];
        node.name        = bone.name;
        node.translation = bone.localPosition;
        node.rotation    = glm::vec4(bone.localRotation.x, bone.localRotation.y,
                                      bone.localRotation.z, bone.localRotation.w);
        node.scale       = bone.localScale;
    }
    for (int i = 0; i < boneCount; ++i) {
        const int parent = skeleton.bones[i].parentIndex;
        if (parent >= 0 && parent < boneCount)
            out.nodes[parent].children.push_back(i);
    }

    // --- Skin: joints[i] == i, a direct 1:1 mapping from JOINTS_0 vertex values to node indices.
    GltfSkin skin;
    skin.joints.resize(boneCount);
    skin.inverseBindMatrices.resize(boneCount);
    for (int i = 0; i < boneCount; ++i) {
        skin.joints[i]              = i;
        skin.inverseBindMatrices[i] = skeleton.bones[i].bindPoseInverse;
    }
    const int skinIndex = static_cast<int>(out.skins.size());
    out.skins.push_back(std::move(skin));

    // --- Vertex accessors, shared across every primitive (only the index accessor differs
    // per submesh -- see the MmdSubMesh loop below).
    const size_t vertCount = mesh.vertices.size();
    std::vector<glm::vec3>  positions(vertCount);
    std::vector<glm::vec3>  normals(vertCount);
    std::vector<glm::vec2>  texCoords(vertCount);
    std::vector<glm::ivec4> joints(vertCount);
    std::vector<glm::vec4>  weights(vertCount);
    for (size_t i = 0; i < vertCount; ++i) {
        const auto& v = mesh.vertices[i];
        positions[i] = v.position;
        normals[i]   = v.normal;
        texCoords[i] = v.texCoord;
        joints[i]    = v.boneIndices;
        weights[i]   = v.boneWeights;
    }

    const int posAcc     = appendAccessor(out, positions, GltfComponentType::Float,       GltfAccessorType::Vec3);
    const int normAcc    = appendAccessor(out, normals,   GltfComponentType::Float,       GltfAccessorType::Vec3);
    const int texAcc     = appendAccessor(out, texCoords, GltfComponentType::Float,       GltfAccessorType::Vec2);
    const int jointsAcc  = appendAccessor(out, joints,    GltfComponentType::UnsignedInt, GltfAccessorType::Vec4);
    const int weightsAcc = appendAccessor(out, weights,   GltfComponentType::Float,       GltfAccessorType::Vec4);

    // --- Morph targets: one dense POSITION-displacement accessor per MorphTarget, expanded from
    // its sparse MorphVertex deltas (unlisted vertices default to a zero offset). Every primitive
    // below shares the same base vertex buffer (posAcc etc.), so these accessors -- and their
    // indices within each primitive's `targets` -- are shared across primitives too.
    std::vector<GltfMorphTarget> meshTargets;
    meshTargets.reserve(morphTargets.size());
    for (const auto& target : morphTargets) {
        std::vector<glm::vec3> deltas(vertCount, glm::vec3{0.f});
        for (const auto& mv : target.deltas) {
            if (mv.vertexIndex >= 0 && static_cast<size_t>(mv.vertexIndex) < vertCount)
                deltas[mv.vertexIndex] = mv.positionOffset;
        }
        GltfMorphTarget gt;
        gt.positionAccessor = appendAccessor(out, deltas, GltfComponentType::Float, GltfAccessorType::Vec3);
        meshTargets.push_back(gt);
    }

    // --- Materials + primitives ---
    GltfMesh gltfMesh;

    if (subMeshes.empty()) {
        out.materials.push_back(GltfMaterial{});

        GltfPrimitive prim;
        prim.positionAccessor  = posAcc;
        prim.normalAccessor    = normAcc;
        prim.texCoord0Accessor = texAcc;
        prim.jointsAccessor    = jointsAcc;
        prim.weightsAccessor   = weightsAcc;
        prim.indicesAccessor   = appendAccessor(out, mesh.indices, GltfComponentType::UnsignedInt, GltfAccessorType::Scalar);
        prim.materialIndex     = 0;
        gltfMesh.primitives.push_back(prim);
    } else {
        for (const auto& sub : subMeshes) {
            GltfMaterial mat;
            mat.pbrMetallicRoughness.baseColorFactor = sub.props.diffuse;
            mat.pbrMetallicRoughness.metallicFactor  = 0.f; // MMD materials are diffuse+specular, not metallic/roughness
            mat.pbrMetallicRoughness.roughnessFactor  = 1.f;
            mat.doubleSided = (sub.props.drawFlags & 0x01) != 0; // PMX drawFlags bit0 = two-sided

            if (sub.textureIdx >= 0 && sub.textureIdx < static_cast<int>(texturePaths.size())) {
                const auto absPath = std::filesystem::path(modelDir) / texturePaths[sub.textureIdx];
                GltfImage img;
                if (decodeImageFile(img, absPath)) {
                    const int imgIdx = static_cast<int>(out.images.size());
                    out.images.push_back(std::move(img));
                    if (out.samplers.empty()) out.samplers.push_back(GltfSampler{});
                    GltfTexture tex;
                    tex.imageIndex   = imgIdx;
                    tex.samplerIndex = 0;
                    const int texIdx = static_cast<int>(out.textures.size());
                    out.textures.push_back(tex);
                    mat.pbrMetallicRoughness.baseColorTexture.index = texIdx;
                }
            }

            const int matIdx = static_cast<int>(out.materials.size());
            out.materials.push_back(std::move(mat));

            std::vector<uint32_t> sliceIndices(
                mesh.indices.begin() + sub.indexOffset,
                mesh.indices.begin() + sub.indexOffset + sub.indexCount);

            GltfPrimitive prim;
            prim.positionAccessor  = posAcc;
            prim.normalAccessor    = normAcc;
            prim.texCoord0Accessor = texAcc;
            prim.jointsAccessor    = jointsAcc;
            prim.weightsAccessor   = weightsAcc;
            prim.indicesAccessor   = appendAccessor(out, sliceIndices, GltfComponentType::UnsignedInt, GltfAccessorType::Scalar);
            prim.materialIndex     = matIdx;
            gltfMesh.primitives.push_back(prim);
        }
    }

    if (!meshTargets.empty()) {
        for (auto& prim : gltfMesh.primitives)
            prim.targets = meshTargets;
        gltfMesh.weights.assign(meshTargets.size(), 0.f);
    }

    const int meshIndex = static_cast<int>(out.meshes.size());
    out.meshes.push_back(std::move(gltfMesh));

    // Dedicated identity-transform node holding the mesh+skin. Per glTF spec (and
    // GltfSceneRenderer::traverseNode()'s matching special case) a skinned node's own transform
    // is never applied to its vertices, so this node's identity translation/rotation/scale are
    // never actually read -- left at the default rather than reused from a bone node, to keep
    // "mesh position" and "joint hierarchy" as clearly separate concerns in the data.
    GltfNode meshNode;
    meshNode.name       = "SkinnedMeshRoot";
    meshNode.meshIndex  = meshIndex;
    meshNode.skin       = skinIndex;
    if (!meshTargets.empty())
        meshNode.weights.assign(meshTargets.size(), 0.f);
    const int meshNodeIndex = static_cast<int>(out.nodes.size());
    out.nodes.push_back(std::move(meshNode));

    GltfScene scene;
    scene.nodes.push_back(meshNodeIndex);
    for (int root : skeleton.rootBoneIndices())
        scene.nodes.push_back(root);
    out.scenes.push_back(std::move(scene));
    out.defaultScene = 0;

    // --- Bone animation channels: one Translation/Rotation/Scale sampler+channel per BoneChannel
    // component that has keys, targeting the same bone-index-as-node-index nodes built above.
    // Position keys are a delta from the bind pose (see Phantom::Animation::Animator::computeFK()'s
    // matching convention, Animator.cpp:88-101) so bone.localPosition is added back in here;
    // rotation/scale keys are already absolute (PMXConverter always sets bind-pose
    // localRotation/localScale to identity).
    GltfAnimation anim;
    for (const auto& ch : bakedClip.channels) {
        if (ch.boneIndex < 0 || ch.boneIndex >= boneCount) continue;
        const Bone& bone = skeleton.bones[ch.boneIndex];

        if (!ch.positionKeys.empty()) {
            std::vector<float>     times;
            std::vector<glm::vec3> values;
            times.reserve(ch.positionKeys.size());
            values.reserve(ch.positionKeys.size());
            for (const auto& key : ch.positionKeys) {
                times.push_back(key.time);
                values.push_back(bone.localPosition + key.value);
            }
            GltfAnimationSampler sampler;
            sampler.input  = appendAccessor(out, times,  GltfComponentType::Float, GltfAccessorType::Scalar);
            sampler.output = appendAccessor(out, values, GltfComponentType::Float, GltfAccessorType::Vec3);
            const int samplerIdx = static_cast<int>(anim.samplers.size());
            anim.samplers.push_back(sampler);
            anim.channels.push_back({samplerIdx, {ch.boneIndex, GltfAnimationPath::Translation}});
        }

        if (!ch.rotationKeys.empty()) {
            std::vector<float>     times;
            std::vector<glm::vec4> values;
            times.reserve(ch.rotationKeys.size());
            values.reserve(ch.rotationKeys.size());
            for (const auto& key : ch.rotationKeys) {
                times.push_back(key.time);
                values.push_back(glm::vec4(key.value.x, key.value.y, key.value.z, key.value.w));
            }
            GltfAnimationSampler sampler;
            sampler.input  = appendAccessor(out, times,  GltfComponentType::Float, GltfAccessorType::Scalar);
            sampler.output = appendAccessor(out, values, GltfComponentType::Float, GltfAccessorType::Vec4);
            const int samplerIdx = static_cast<int>(anim.samplers.size());
            anim.samplers.push_back(sampler);
            anim.channels.push_back({samplerIdx, {ch.boneIndex, GltfAnimationPath::Rotation}});
        }

        if (!ch.scaleKeys.empty()) {
            std::vector<float>     times;
            std::vector<glm::vec3> values;
            times.reserve(ch.scaleKeys.size());
            values.reserve(ch.scaleKeys.size());
            for (const auto& key : ch.scaleKeys) {
                times.push_back(key.time);
                values.push_back(key.value);
            }
            GltfAnimationSampler sampler;
            sampler.input  = appendAccessor(out, times,  GltfComponentType::Float, GltfAccessorType::Scalar);
            sampler.output = appendAccessor(out, values, GltfComponentType::Float, GltfAccessorType::Vec3);
            const int samplerIdx = static_cast<int>(anim.samplers.size());
            anim.samplers.push_back(sampler);
            anim.channels.push_back({samplerIdx, {ch.boneIndex, GltfAnimationPath::Scale}});
        }
    }

    // --- Morph weights channel: a single Weights channel on the mesh node, sampling the dense,
    // pre-merged timeline from MmdAnimationBaker::bakeMorphWeights() (one flattened
    // targetCount-wide sample per merged keyframe time).
    if (!bakedMorphWeights.empty() && !meshTargets.empty()) {
        const size_t targetCount = meshTargets.size();
        std::vector<float> times;
        std::vector<float> flatWeights;
        times.reserve(bakedMorphWeights.size());
        flatWeights.reserve(bakedMorphWeights.size() * targetCount);
        for (const auto& [time, weights] : bakedMorphWeights) {
            times.push_back(time);
            for (size_t i = 0; i < targetCount; ++i)
                flatWeights.push_back(i < weights.size() ? weights[i] : 0.f);
        }
        GltfAnimationSampler sampler;
        sampler.input  = appendAccessor(out, times,       GltfComponentType::Float, GltfAccessorType::Scalar);
        sampler.output = appendAccessor(out, flatWeights, GltfComponentType::Float, GltfAccessorType::Scalar);
        const int samplerIdx = static_cast<int>(anim.samplers.size());
        anim.samplers.push_back(sampler);
        anim.channels.push_back({samplerIdx, {meshNodeIndex, GltfAnimationPath::Weights}});
    }

    if (!anim.channels.empty())
        out.animations.push_back(std::move(anim));

    return true;
}
