#include "CharacterPoseBake.h"

#include "GltfAccessorView.h"
#include "GltfMorphApply.h"
#include "GltfNodeTransform.h"
#include "GltfAnimationEvaluator.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <cmath>
#include <map>

namespace Phantom::Gltf {

std::vector<BakedPrimitive> bakeSkinnedPose(const CharacterPoseSource& src, double framePosition, double fps)
{
    std::vector<BakedPrimitive> result;
    if (!src.doc) return result;
    const GltfDocument& doc = *src.doc;

    // Same evaluation as the viewport (CharacterPose.h): the placed clip, then the authored bone deltas.
    std::vector<glm::mat4> skin;
    if (!doc.skins.empty())
        skin = evaluateCharacterSkin(src, /*skinIndex=*/0, framePosition, fps);

    // Flattens meshes[]/primitives[] in the same traversal order as
    // PhantomStudioSkinnedRenderer::updateMorphs()/PhantomStudioScene.cpp's countMorphTargets(), so
    // the manual override's indices line up.
    int flatMorphIndex = 0;

    for (size_t meshIdx = 0; meshIdx < doc.meshes.size(); ++meshIdx) {
        const GltfMesh& mesh = doc.meshes[meshIdx];
        const int nodeIndex = findNodeForMesh(doc, static_cast<int>(meshIdx));

        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            const GltfPrimitive& prim = mesh.primitives[primIdx];
            if (prim.positionAccessor < 0) continue;

            const std::vector<float> weights = evaluateCharacterMorphWeights(
                src, nodeIndex, static_cast<int>(prim.targets.size()), flatMorphIndex, framePosition, fps);
            flatMorphIndex += static_cast<int>(prim.targets.size());

            const std::vector<glm::vec3> morphedPositions =
                prim.targets.empty() ? std::vector<glm::vec3>() : applyMorphs(doc, prim, weights);

            GltfAccessorView posView(doc, prim.positionAccessor);
            const size_t vertexCount = posView.count();

            const bool hasNormal = prim.normalAccessor >= 0;
            const bool hasTex    = prim.texCoord0Accessor >= 0;
            const bool hasJoints = prim.jointsAccessor >= 0 && prim.weightsAccessor >= 0 && !skin.empty();

            GltfAccessorView normView   (doc, hasNormal ? prim.normalAccessor    : prim.positionAccessor);
            GltfAccessorView texView    (doc, hasTex    ? prim.texCoord0Accessor : prim.positionAccessor);
            GltfAccessorView jointsView (doc, hasJoints ? prim.jointsAccessor    : prim.positionAccessor);
            GltfAccessorView weightsView(doc, hasJoints ? prim.weightsAccessor   : prim.positionAccessor);

            BakedPrimitive baked;
            baked.materialIndex = prim.materialIndex;
            baked.mesh.vertices.resize(vertexCount);

            for (size_t i = 0; i < vertexCount; ++i) {
                const glm::vec3 basePos = morphedPositions.empty() ? posView.get<glm::vec3>(i) : morphedPositions[i];
                const glm::vec3 baseNrm = hasNormal ? normView.get<glm::vec3>(i) : glm::vec3(0.f, 1.f, 0.f);

                glm::vec3 pos = basePos;
                glm::vec3 nrm = baseNrm;
                if (hasJoints) {
                    const glm::uvec4 j = readJointIndices(jointsView, doc.accessors[prim.jointsAccessor], i);
                    const glm::vec4  w = weightsView.get<glm::vec4>(i);
                    glm::mat4 skinMat(0.f);
                    for (int c = 0; c < 4; ++c) {
                        if (w[c] == 0.f) continue;
                        // Reject malformed weighted joints; unused slots must never dereference skin.
                        if (j[c] >= skin.size()) return {};
                        skinMat += w[c] * skin[j[c]];
                    }
                    pos = glm::vec3(skinMat * glm::vec4(basePos, 1.0f));
                    nrm = glm::inverseTranspose(glm::mat3(skinMat)) * baseNrm;
                }
                const float nrmLenSq = glm::dot(nrm, nrm);
                if (nrmLenSq > 1e-12f) nrm *= (1.0f / std::sqrt(nrmLenSq));

                const glm::vec2 uv = hasTex ? texView.get<glm::vec2>(i) : glm::vec2(0.f, 0.f);

                Graphics::MeshVertex& mv = baked.mesh.vertices[i];
                mv.pos[0] = pos.x; mv.pos[1] = pos.y; mv.pos[2] = pos.z;
                mv.nrm[0] = nrm.x; mv.nrm[1] = nrm.y; mv.nrm[2] = nrm.z;
                mv.uv[0]  = uv.x;  mv.uv[1]  = uv.y;
            }

            if (prim.indicesAccessor >= 0) {
                const GltfAccessor& idxAcc = doc.accessors[prim.indicesAccessor];
                GltfAccessorView idxView(doc, prim.indicesAccessor);
                baked.mesh.indices.resize(idxView.count());
                for (size_t i = 0; i < idxView.count(); ++i)
                    baked.mesh.indices[i] = readIndexValue(idxView, idxAcc, i);
            } else {
                baked.mesh.indices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; ++i)
                    baked.mesh.indices[i] = static_cast<uint32_t>(i);
            }

            if (!baked.mesh.vertices.empty() && !baked.mesh.indices.empty())
                result.push_back(std::move(baked));
        }
    }
    return result;
}

CharacterChannelSamples sampleCharacterChannels(const CharacterPoseSource& src, int startFrame, int endFrame, double fps)
{
    CharacterChannelSamples out;
    if (!src.doc || endFrame < startFrame) return out;
    const GltfDocument& doc = *src.doc;

    // Parent of every node, and the bind-pose globals (for ancestors that are not joints).
    std::vector<int> parent(doc.nodes.size(), -1);
    for (size_t i = 0; i < doc.nodes.size(); ++i)
        for (int c : doc.nodes[i].children)
            if (c >= 0 && c < static_cast<int>(parent.size())) parent[c] = static_cast<int>(i);
    const std::vector<glm::mat4> staticGlobal = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, -1, 0.f);

    struct Track {
        CharacterJointSamples samples;
        glm::quat prev{ 1.f, 0.f, 0.f, 0.f };
        bool first = true;
    };
    std::map<int, Track> joints;   // node index -> track
    for (const auto& skin : doc.skins)
        for (int j : skin.joints)
            if (j >= 0 && j < static_cast<int>(doc.nodes.size())) joints[j].samples.node = j;

    for (int f = startFrame; f <= endFrame; ++f) {
        out.times.push_back(static_cast<float>(f / fps));
        std::map<int, glm::mat4> posed;   // joint node -> posed global
        for (size_t si = 0; si < doc.skins.size(); ++si) {
            const auto skinMats = evaluateCharacterSkin(src, static_cast<int>(si), static_cast<double>(f), fps);
            const auto& skin = doc.skins[si];
            for (size_t ji = 0; ji < skin.joints.size() && ji < skinMats.size(); ++ji)
                posed[skin.joints[ji]] = skinMats[ji] * glm::inverse(skin.inverseBindMatrices[ji]);
        }
        for (auto& [node, track] : joints) {
            auto it = posed.find(node);
            if (it == posed.end()) continue;
            const int p = parent[node];
            glm::mat4 parentGlobal(1.f);
            if (p >= 0) {
                auto pit = posed.find(p);
                parentGlobal = pit != posed.end() ? pit->second : staticGlobal[p];
            }
            const glm::mat4 local = glm::inverse(parentGlobal) * it->second;
            glm::vec3 scale, translation, skew;
            glm::vec4 persp;
            glm::quat rot;
            glm::decompose(local, scale, rot, translation, skew, persp);
            rot = glm::normalize(rot);
            if (!track.first && glm::dot(rot, track.prev) < 0.f) rot = -rot;
            track.prev = rot;
            track.first = false;
            track.samples.translations.push_back(translation);
            track.samples.rotations.push_back({ rot.x, rot.y, rot.z, rot.w });
            track.samples.scales.push_back(scale);
        }
    }
    // A joint has a sample for every time or none (the skins do not change between frames).
    for (auto& [node, track] : joints)
        if (track.samples.translations.size() == out.times.size()) out.joints.push_back(std::move(track.samples));

    // Morph weights: one track per mesh shown by a node. The flat index follows the viewport's mesh/primitive traversal.
    int flat = 0;
    for (size_t mi = 0; mi < doc.meshes.size(); ++mi) {
        const int nodeIndex = findNodeForMesh(doc, static_cast<int>(mi));
        int firstFlat = -1, targetCount = 0;
        for (const auto& p : doc.meshes[mi].primitives) {
            if (!p.targets.empty() && firstFlat < 0) { firstFlat = flat; targetCount = static_cast<int>(p.targets.size()); }
            flat += static_cast<int>(p.targets.size());
        }
        if (nodeIndex < 0 || targetCount == 0) continue;
        CharacterMorphSamples samples;
        samples.node = nodeIndex;
        samples.targetCount = targetCount;
        for (int f = startFrame; f <= endFrame; ++f) {
            const auto w = evaluateCharacterMorphWeights(src, nodeIndex, targetCount, firstFlat, static_cast<double>(f), fps);
            for (int k = 0; k < targetCount; ++k) samples.weights.push_back(k < static_cast<int>(w.size()) ? w[k] : 0.f);
        }
        out.morphs.push_back(std::move(samples));
    }
    return out;
}

} // namespace Phantom::Gltf
