#include <gtest/gtest.h>

#include "../Gltf/CharacterPoseBake.h"

#include <cstdint>
#include <memory>

// CPU pose bake, moved from PhantomStudio's CharacterPoseBakerTest (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 3).
using namespace Phantom::Gltf;

namespace {

template <typename T>
size_t appendData(std::vector<uint8_t>& buf, const std::vector<T>& values) {
    const size_t offset = buf.size();
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(values.data());
    buf.insert(buf.end(), bytes, bytes + values.size() * sizeof(T));
    return offset;
}

int addAccessor(GltfDocument& doc, int bufferIndex, size_t byteOffset, size_t count,
                 GltfAccessorType type, GltfComponentType componentType) {
    GltfBufferView bv;
    bv.bufferIndex = bufferIndex;
    bv.byteOffset = byteOffset;
    bv.byteLength = count * static_cast<size_t>(componentCount(type)) * static_cast<size_t>(componentByteSize(componentType));
    const int bvIdx = static_cast<int>(doc.bufferViews.size());
    doc.bufferViews.push_back(bv);

    GltfAccessor acc;
    acc.bufferViewIndex = bvIdx;
    acc.componentType = componentType;
    acc.type = type;
    acc.count = count;
    const int accIdx = static_cast<int>(doc.accessors.size());
    doc.accessors.push_back(acc);
    return accIdx;
}

// A single quad primitive (4 verts, 2 triangles, normals all +Y) fully bound to joint 0 of a 1-joint skin. Joint node 0 is
// offset by (2,0,0) with an identity inverse-bind-matrix, so the bind-pose skin matrix is a pure translate(2,0,0): baked
// positions must equal the source positions shifted by exactly that offset, and normals must stay unchanged. One morph
// target (POSITION delta (0,1,0) per vertex) is included so tests can exercise the manual morph override.
std::shared_ptr<GltfDocument> buildRiggedQuadDoc() {
    auto doc = std::make_shared<GltfDocument>();
    doc->buffers.push_back({});
    auto& buf = doc->buffers[0].data;

    const std::vector<glm::vec3> positions = {
        { -0.5f, 0.f, -0.5f }, { 0.5f, 0.f, -0.5f }, { 0.5f, 0.f, 0.5f }, { -0.5f, 0.f, 0.5f }
    };
    const std::vector<glm::vec3> normals(4, glm::vec3(0.f, 1.f, 0.f));
    const std::vector<glm::vec3> morphDelta(4, glm::vec3(0.f, 1.f, 0.f));
    std::vector<uint32_t> jointsFlat;
    std::vector<float> weightsFlat;
    for (int i = 0; i < 4; ++i) {
        jointsFlat.insert(jointsFlat.end(), { 0u, 0u, 0u, 0u });
        weightsFlat.insert(weightsFlat.end(), { 1.f, 0.f, 0.f, 0.f });
    }
    const std::vector<uint16_t> indices = { 0, 1, 2, 0, 2, 3 };

    const size_t posOff = appendData(buf, positions);
    const size_t nrmOff = appendData(buf, normals);
    const size_t morphOff = appendData(buf, morphDelta);
    const size_t jointOff = appendData(buf, jointsFlat);
    const size_t weightOff = appendData(buf, weightsFlat);
    const size_t idxOff = appendData(buf, indices);

    const int posAcc = addAccessor(*doc, 0, posOff, 4, GltfAccessorType::Vec3, GltfComponentType::Float);
    const int nrmAcc = addAccessor(*doc, 0, nrmOff, 4, GltfAccessorType::Vec3, GltfComponentType::Float);
    const int morphAcc = addAccessor(*doc, 0, morphOff, 4, GltfAccessorType::Vec3, GltfComponentType::Float);
    const int jointAcc = addAccessor(*doc, 0, jointOff, 4, GltfAccessorType::Vec4, GltfComponentType::UnsignedInt);
    const int weightAcc = addAccessor(*doc, 0, weightOff, 4, GltfAccessorType::Vec4, GltfComponentType::Float);
    const int idxAcc = addAccessor(*doc, 0, idxOff, 6, GltfAccessorType::Scalar, GltfComponentType::UnsignedShort);

    GltfPrimitive prim;
    prim.positionAccessor = posAcc;
    prim.normalAccessor = nrmAcc;
    prim.jointsAccessor = jointAcc;
    prim.weightsAccessor = weightAcc;
    prim.indicesAccessor = idxAcc;
    prim.targets.push_back(GltfMorphTarget{ morphAcc });

    GltfMesh mesh;
    mesh.primitives.push_back(prim);
    doc->meshes.push_back(mesh);

    GltfNode jointNode;
    jointNode.translation = { 2.f, 0.f, 0.f };
    doc->nodes.push_back(jointNode); // index 0

    GltfNode meshNode;
    meshNode.meshIndex = 0;
    doc->nodes.push_back(meshNode); // index 1

    GltfScene scene;
    scene.nodes = { 0, 1 }; // both are roots; the joint node must be reachable for the skin evaluation
    doc->scenes.push_back(scene);
    doc->defaultScene = 0;

    GltfSkin skin;
    skin.joints = { 0 };
    skin.inverseBindMatrices = { glm::mat4(1.f) };
    doc->skins.push_back(skin);

    return doc;
}

} // namespace

TEST(CharacterPoseBake, MissingDocumentReturnsEmpty)
{
    CharacterPoseSource src;
    EXPECT_TRUE(bakeSkinnedPose(src, 0.0, 30.0).empty());
}

TEST(CharacterPoseBake, BindPoseBakesSkinTranslationIntoPositions)
{
    const auto doc = buildRiggedQuadDoc();
    CharacterPoseSource src;
    src.doc = doc.get();
    ASSERT_EQ(-1, src.activeAnimation); // bind pose

    const auto baked = bakeSkinnedPose(src, 0.0, 30.0);
    ASSERT_EQ(1u, baked.size());
    ASSERT_EQ(4u, baked[0].mesh.vertices.size());
    ASSERT_EQ(6u, baked[0].mesh.indices.size());

    // Joint 0's static translation (2,0,0) with identity inverse-bind-matrix -> every vertex shifts by exactly (2,0,0);
    // normals (translation only) are unaffected.
    const float expectedX[4] = { -0.5f + 2.f, 0.5f + 2.f, 0.5f + 2.f, -0.5f + 2.f };
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(expectedX[i], baked[0].mesh.vertices[i].pos[0], 1e-4f);
        EXPECT_NEAR(0.f, baked[0].mesh.vertices[i].pos[1], 1e-4f);
        EXPECT_NEAR(0.f, baked[0].mesh.vertices[i].nrm[0], 1e-4f);
        EXPECT_NEAR(1.f, baked[0].mesh.vertices[i].nrm[1], 1e-4f);
    }
}

TEST(CharacterPoseBake, MorphWeightOverrideDisplacesPositionsBeforeSkinning)
{
    const auto doc = buildRiggedQuadDoc();
    const std::vector<float> manual = { 1.0f };
    CharacterPoseSource src;
    src.doc = doc.get();
    src.morphOverride = &manual;

    const auto baked = bakeSkinnedPose(src, 0.0, 30.0);
    ASSERT_EQ(1u, baked.size());

    // Base position + morph delta (0,1,0) + skin translation (2,0,0).
    EXPECT_NEAR(-0.5f + 2.f, baked[0].mesh.vertices[0].pos[0], 1e-4f);
    EXPECT_NEAR(1.f, baked[0].mesh.vertices[0].pos[1], 1e-4f);
}

TEST(CharacterPoseBake, IndicesArePreservedInOriginalWindingOrder)
{
    const auto doc = buildRiggedQuadDoc();
    CharacterPoseSource src;
    src.doc = doc.get();

    const auto baked = bakeSkinnedPose(src, 0.0, 30.0);
    ASSERT_EQ(1u, baked.size());
    const std::vector<uint32_t> expected = { 0, 1, 2, 0, 2, 3 };
    ASSERT_EQ(expected.size(), baked[0].mesh.indices.size());
    for (size_t i = 0; i < expected.size(); ++i)
        EXPECT_EQ(expected[i], baked[0].mesh.indices[i]);
    EXPECT_EQ(-1, baked[0].materialIndex);
}
