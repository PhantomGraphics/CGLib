#include <gtest/gtest.h>

#include "../Gltf/CharacterPose.h"
#include "../Gltf/GltfAccessorBuilder.h"
#include "../Gltf/GltfAnimationEvaluator.h"

#include <cmath>
#include <memory>

// Character pose evaluation, moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 3).
using namespace Phantom::Gltf;
using namespace Phantom::Animation;

namespace {

// A synthetic two-bone chain with one (empty) morph target: root (node 0) at the origin, child (node 1) one unit up, both
// skin joints with identity inverse binds.
std::shared_ptr<GltfDocument> chainDoc() {
    auto doc = std::make_shared<GltfDocument>();
    doc->nodes.resize(2);
    doc->nodes[0].name = "Root";
    doc->nodes[0].children = { 1 };
    doc->nodes[1].name = "Arm";
    doc->nodes[1].translation = { 0.f, 1.f, 0.f };
    GltfScene scene;
    scene.nodes = { 0 };
    doc->scenes.push_back(scene);
    GltfSkin skin;
    skin.joints = { 0, 1 };
    skin.inverseBindMatrices = { glm::mat4(1.f), glm::mat4(1.f) };
    doc->skins.push_back(skin);
    GltfMesh mesh;
    GltfPrimitive prim;
    prim.targets.resize(1);
    mesh.primitives.push_back(prim);
    doc->meshes.push_back(mesh);
    return doc;
}

glm::vec3 jointPosition(const std::vector<glm::mat4>& skin, size_t joint) { return glm::vec3(skin[joint][3]); }

// The chain's root slides along +x in clip 0 (0 -> 2 over one second) and along +y in clip 1 (0 -> 4).
std::shared_ptr<GltfDocument> twoClipChainDoc() {
    auto doc = chainDoc();
    auto slide = [&](const char* name, glm::vec3 end) {
        GltfAnimation anim;
        anim.name = name;
        std::vector<float> times = { 0.f, 1.f };
        std::vector<glm::vec3> values = { glm::vec3(0.f), end };
        GltfAnimationSampler sampler;
        sampler.input = appendAccessor(*doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);
        sampler.output = appendAccessor(*doc, values, GltfComponentType::Float, GltfAccessorType::Vec3);
        anim.samplers.push_back(sampler);
        anim.channels.push_back({ 0, { 0, GltfAnimationPath::Translation } });
        doc->animations.push_back(anim);
    };
    slide("SlideX", glm::vec3(2.f, 0.f, 0.f));
    slide("SlideY", glm::vec3(0.f, 4.f, 0.f));
    return doc;
}

BoneTrack bone(int node, const char* name) {
    BoneTrack b;
    b.node = node;
    b.name = name;
    return b;
}

} // namespace

TEST(CharacterPose, UnauthoredCharacterMatchesTheEvaluatorExactly) {
    const auto doc = chainDoc();
    CharacterPoseSource src;
    src.doc = doc.get();
    const auto mine = evaluateCharacterSkin(src, 0, 12.0, 30.0);
    const auto reference = GltfAnimationEvaluator::evaluateSkin(*doc, -1, 0, 0.f);
    ASSERT_EQ(reference.size(), mine.size());
    for (size_t j = 0; j < mine.size(); ++j)
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) EXPECT_EQ(reference[j][col][row], mine[j][col][row]);

    // An explicitly empty authored block takes the same path.
    CharacterAnimation none;
    src.authored = &none;
    EXPECT_EQ(mine, evaluateCharacterSkin(src, 0, 12.0, 30.0));
}

TEST(CharacterPose, OutOfRangeSkinOrMissingDocumentIsEmpty) {
    const auto doc = chainDoc();
    CharacterPoseSource src;
    EXPECT_TRUE(evaluateCharacterSkin(src, 0, 0.0, 30.0).empty());
    src.doc = doc.get();
    EXPECT_TRUE(evaluateCharacterSkin(src, 1, 0.0, 30.0).empty());
    EXPECT_TRUE(evaluateCharacterSkin(src, -1, 0.0, 30.0).empty());
    EXPECT_DOUBLE_EQ(0.0, characterClipTime(src, 10.0, 30.0));
    EXPECT_DOUBLE_EQ(0.0, characterClipDuration(src));
}

TEST(CharacterPose, BoneDeltaMovesTheBoneAndItsChildren) {
    const auto doc = chainDoc();
    CharacterAnimation anim;
    anim.bones = { bone(0, "Root") };
    anim.bones[0].pose.rotationEulerDeg = { 0.f, 0.f, 90.f };  // turn the root a quarter about Z
    CharacterPoseSource src;
    src.doc = doc.get();
    src.authored = &anim;
    auto skin = evaluateCharacterSkin(src, 0, 0.0, 30.0);
    EXPECT_NEAR(-1.f, jointPosition(skin, 1).x, 1e-5f);  // the arm (0,1,0) swings to (-1,0,0)
    EXPECT_NEAR(0.f, jointPosition(skin, 1).y, 1e-5f);

    anim.bones.push_back(bone(1, "Arm"));
    anim.bones[1].pose.translation = { 0.f, 0.5f, 0.f };  // along the arm's own (now rotated) axis
    skin = evaluateCharacterSkin(src, 0, 0.0, 30.0);
    EXPECT_NEAR(-1.5f, jointPosition(skin, 1).x, 1e-5f);
}

TEST(CharacterPose, KeyedBoneRotationOverridesTheStaticPoseAndInterpolates) {
    const auto doc = chainDoc();
    CharacterAnimation anim;
    anim.bones = { bone(0, "Root") };
    TrackKey a; a.frame = 0;  a.value = { 0, 0, 0 };
    TrackKey b; b.frame = 10; b.value = { 0, 0, 90 };
    anim.bones[0].tracks.track(AnimChannel::Rotation) = { a, b };
    CharacterPoseSource src;
    src.doc = doc.get();
    src.authored = &anim;

    auto skin = evaluateCharacterSkin(src, 0, 0.0, 30.0);
    EXPECT_NEAR(0.f, jointPosition(skin, 1).x, 1e-5f);
    skin = evaluateCharacterSkin(src, 0, 10.0, 30.0);
    EXPECT_NEAR(-1.f, jointPosition(skin, 1).x, 1e-5f);
    skin = evaluateCharacterSkin(src, 0, 5.0, 30.0);  // 45 degrees
    EXPECT_NEAR(-std::sqrt(0.5f), jointPosition(skin, 1).x, 1e-4f);
    EXPECT_NEAR(std::sqrt(0.5f), jointPosition(skin, 1).y, 1e-4f);
}

TEST(CharacterPose, MorphKeysBeatTheManualOverrideAndAreClamped) {
    const auto doc = chainDoc();
    std::vector<float> manual = { 0.9f };
    CharacterAnimation anim;
    CharacterPoseSource src;
    src.doc = doc.get();
    src.authored = &anim;
    src.morphOverride = &manual;
    EXPECT_FLOAT_EQ(0.9f, evaluateCharacterMorphWeights(src, -1, 1, 0, 0.0, 30.0)[0]);  // the manual override

    TrackKey a; a.frame = 0;  a.value = { 0.f, 0, 0 };
    TrackKey b; b.frame = 10; b.value = { 2.f, 0, 0 };   // 2 is clamped to 1
    anim.morphs[0].track(AnimChannel::Translation) = { a, b };
    EXPECT_NEAR(0.5f, evaluateCharacterMorphWeights(src, -1, 1, 0, 2.5, 30.0)[0], 1e-5f);  // keys win over the override (2 * 0.25)
    EXPECT_FLOAT_EQ(1.f, evaluateCharacterMorphWeights(src, -1, 1, 0, 10.0, 30.0)[0]);

    // A zero override does not win, and a flat index past the override array is just not overridden.
    anim.morphs.clear();
    manual[0] = 0.f;
    EXPECT_FLOAT_EQ(0.f, evaluateCharacterMorphWeights(src, -1, 1, 0, 0.0, 30.0)[0]);
    EXPECT_FLOAT_EQ(0.f, evaluateCharacterMorphWeights(src, -1, 1, 5, 0.0, 30.0)[0]);
}

TEST(CharacterPose, ClipPlacementMapsSceneTimeToClipTime) {
    const auto doc = twoClipChainDoc();
    CharacterAnimation anim;
    anim.clip.startFrame = 30;  // starts at 1 s
    CharacterPoseSource src;
    src.doc = doc.get();
    src.activeAnimation = 0;
    src.authored = &anim;
    EXPECT_DOUBLE_EQ(1.0, characterClipDuration(src));
    EXPECT_DOUBLE_EQ(0.0, characterClipTime(src, 15.0, 30.0));   // before the start: the first pose
    EXPECT_DOUBLE_EQ(0.5, characterClipTime(src, 45.0, 30.0));
    auto skin = evaluateCharacterSkin(src, 0, 45.0, 30.0);       // half way along clip 0: x = 1
    EXPECT_NEAR(1.f, jointPosition(skin, 0).x, 1e-5f);
}

TEST(ClipBlend, CrossFadeMixesTheTwoClipsPerBone) {
    const auto doc = twoClipChainDoc();
    CharacterAnimation anim;
    anim.blend.clip = 1;
    anim.blend.fadeStart = 0;
    anim.blend.fadeFrames = 30;
    CharacterPoseSource src;
    src.doc = doc.get();
    src.activeAnimation = 0;
    src.authored = &anim;

    // frame 0: weight 0 = the main clip only (both clips are at their first pose here anyway)
    auto skin = evaluateCharacterSkin(src, 0, 0.0, 30.0);
    EXPECT_NEAR(0.f, jointPosition(skin, 0).x, 1e-5f);
    // frame 15: A is at x = 1, B at y = 2, weight 0.5 -> (0.5, 1, 0); the child sits one unit above its root
    skin = evaluateCharacterSkin(src, 0, 15.0, 30.0);
    EXPECT_NEAR(0.5f, jointPosition(skin, 0).x, 1e-4f);
    EXPECT_NEAR(1.0f, jointPosition(skin, 0).y, 1e-4f);
    EXPECT_NEAR(2.0f, jointPosition(skin, 1).y, 1e-4f);
    // frame 30 and later: the blended clip only (y = 4, x back to 0)
    skin = evaluateCharacterSkin(src, 0, 30.0, 30.0);
    EXPECT_NEAR(0.f, jointPosition(skin, 0).x, 1e-4f);
    EXPECT_NEAR(4.f, jointPosition(skin, 0).y, 1e-4f);

    // a late fade holds the main clip until it starts: at frame 15 with fadeStart 30 only clip 0 plays
    anim.blend.fadeStart = 30;
    skin = evaluateCharacterSkin(src, 0, 15.0, 30.0);
    EXPECT_NEAR(1.f, jointPosition(skin, 0).x, 1e-4f);
    EXPECT_NEAR(0.f, jointPosition(skin, 0).y, 1e-4f);
}

TEST(CharacterPose, BindPoseBoundsCoverEveryPrimitive) {
    auto doc = chainDoc();
    glm::vec3 lo(0.f), hi(0.f);
    EXPECT_FALSE(computeBindPoseBounds(*doc, lo, hi));   // no geometry
    const std::vector<glm::vec3> positions = { { -1.f, 0.f, 2.f }, { 3.f, 4.f, -5.f } };
    doc->meshes[0].primitives[0].positionAccessor = appendAccessor(*doc, positions, GltfComponentType::Float, GltfAccessorType::Vec3);
    ASSERT_TRUE(computeBindPoseBounds(*doc, lo, hi));
    EXPECT_EQ(glm::vec3(-1.f, 0.f, -5.f), lo);
    EXPECT_EQ(glm::vec3(3.f, 4.f, 2.f), hi);
}
