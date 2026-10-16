#include "CharacterPoseBake.h"

#include "GltfAccessorView.h"
#include "GltfMorphApply.h"
#include "GltfNodeTransform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>

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
                    const glm::ivec4 j = jointsView.get<glm::ivec4>(i);
                    const glm::vec4  w = weightsView.get<glm::vec4>(i);
                    const glm::mat4 skinMat =
                        w.x * skin[j.x] + w.y * skin[j.y] + w.z * skin[j.z] + w.w * skin[j.w];
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

} // namespace Phantom::Gltf
