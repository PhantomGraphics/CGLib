#include "AnimationViewApp.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "imgui.h"

#include "CGLib/File/File/PMXFileReader.h"
#include "CGLib/GltfRenderer/Gltf/GltfAnimationEvaluator.h"
#include "CGLib/GltfRenderer/Gltf/GltfMorphApply.h"
#include "CGLib/GltfRenderer/Gltf/MmdToGltfConverter.h"

#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace Phantom::Animation;
using namespace Phantom::Gltf;

// Converts a UTF-8 std::string to std::filesystem::path (C++20 compatible).
static std::filesystem::path u8ToPath(const std::string& utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

// ============================================================
//  AnimationViewApp
// ============================================================

AnimationViewApp::AnimationViewApp(int w, int h, const std::string& title)
    : ::VKG::VkAppBase(w, h, title)
{
    dispatcher_.setWorld(&world_);
    scenarioBrowser_.setHost(this);
    scenarioBrowser_.setDefaultFolder("scenarios");

    panel_.init(&world_);
    add(&panel_);
    add(&scenarioBrowser_);
}

bool AnimationViewApp::loadScenario(const std::string& jsonPath)
{
    return runner_.load(jsonPath);
}

// ============================================================
//  VkAppBase hooks
// ============================================================

void AnimationViewApp::onInit()
{
    {
        GltfSceneRenderer::Shaders s;
        s.vertSpv = ::VKG::loadSPVRepo("shaders/gltf.vert.spv");
        s.fragSpv = ::VKG::loadSPVRepo("shaders/gltf.frag.spv");
        // Skybox/IBL/shadow shaders left empty on purpose: AnimationView is a plain model/motion
        // viewer, not a lighting testbed (GltfSceneRenderer treats empty = skip, see its Shaders
        // struct comment).
        sceneRenderer_.setShaders(std::move(s));
    }

    ::VKG::VkAppBase::onInit();
    sceneRenderer_.setExtent(getExtent());
    sceneRenderer_.onInit(getContext(), getCommandPool(), getRenderPass(), MAX_FRAMES_IN_FLIGHT);
    sceneRenderer_.setUseIBL(false);

    lastFrameTime_ = std::chrono::steady_clock::now();

    setupWindowCallbacks();
}

void AnimationViewApp::onSwapChainCreated()
{
    sceneRenderer_.setExtent(getExtent());
}

void AnimationViewApp::onUpdate(uint32_t frameIndex)
{
    dispatcher_.processQueue();

    if (runner_.isActive()) {
        auto responses = dispatcher_.collectResponses();
        if (runner_.tick(dispatcher_, responses)) {
            if (runner_.hasFailed()) {
                std::fprintf(stderr, "[Scenario] FAILED: %s\n", runner_.failMessage().c_str());
                exitCode_ = 1;
            } else {
                std::fprintf(stdout, "[Scenario] PASSED (%zu steps)\n", runner_.stepCount());
                exitCode_ = 0;
            }
            if (exitOnComplete_) getWindow().close();
        }
    }

    tryLoadModel();

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    tickAnimation(dt);

    ::VKG::VkAppBase::onUpdate(frameIndex);
}

void AnimationViewApp::onImGui()
{
    ::VKG::VkAppBase::onImGui();
}

void AnimationViewApp::onCleanup()
{
    sceneRenderer_.onCleanup(getDevice());
    ::VKG::VkAppBase::onCleanup();
}

// ============================================================
//  Private helpers
// ============================================================

void AnimationViewApp::tickAnimation(float dt)
{
    if (world_.playing) {
        world_.currentTime += dt * world_.speed;
        if (world_.loop && world_.duration > 0.f) {
            while (world_.currentTime > world_.duration)
                world_.currentTime -= world_.duration;
        } else if (world_.currentTime >= world_.duration) {
            world_.currentTime = world_.duration;
            world_.playing     = false;
        }
        world_.dirty = true;
    }

    if (world_.dirty) {
        if (world_.skinIndex >= 0 && world_.skinIndex < (int)world_.document.skins.size()) {
            auto skinMatrices = GltfAnimationEvaluator::evaluateSkin(
                world_.document, world_.animationIndex, world_.skinIndex, world_.currentTime);
            sceneRenderer_.updateSkinMatrices(std::move(skinMatrices));
        }

        // Morph weights: evaluate + blend + push to the GPU for every primitive that has morph
        // targets (Phase 7's applyMorphs()/updateMorphedPositions() plumbing -- Universe's
        // MmdCharacterModel never needs this since its character has no morphs, so this is the
        // one place in the repo currently exercising it).
        if (!world_.document.meshes.empty()) {
            const GltfMesh& mesh = world_.document.meshes[0];
            for (int nodeIdx = 0; nodeIdx < (int)world_.document.nodes.size(); ++nodeIdx) {
                if (world_.document.nodes[nodeIdx].meshIndex != 0) continue;
                for (int primIdx = 0; primIdx < (int)mesh.primitives.size(); ++primIdx) {
                    const GltfPrimitive& prim = mesh.primitives[primIdx];
                    if (prim.targets.empty()) continue;
                    auto weights = GltfAnimationEvaluator::evaluateMorphWeights(
                        world_.document, world_.animationIndex, nodeIdx,
                        (int)prim.targets.size(), world_.currentTime);
                    auto positions = applyMorphs(world_.document, prim, weights);
                    sceneRenderer_.updateMorphedPositions(0, primIdx, positions);
                }
                break; // SkeletonGltfConverter emits exactly one such node
            }
        }

        world_.dirty = false;
    }

    sceneRenderer_.setVisible(world_.showMesh);
}

void AnimationViewApp::tryLoadModel()
{
    const bool modelChanged  = !world_.loadedModelPath.empty() && world_.loadedModelPath != currentModelPath_;
    const bool motionChanged = world_.loadedMotionPath != currentVMDPath_;
    if (!modelChanged && !motionChanged) return;

    if (modelChanged)  currentModelPath_ = world_.loadedModelPath;
    if (motionChanged) currentVMDPath_   = world_.loadedMotionPath;
    if (currentModelPath_.empty()) return; // a VMD alone can't be previewed yet

    // Direct PMX read for parse-failure detail (PMXParseStats) -- MmdToGltfConverter::convert()'s
    // plain bool doesn't expose section-level failure info the "PMX Debug" panel wants. Only
    // re-parse for stats when the *model* path itself changed (a VMD-only reload re-reads the
    // same PMX again below regardless, via MmdToGltfConverter, but there is no need to redo the
    // stats read for that).
    if (modelChanged) {
        Phantom::File::PMXFileReader reader;
        const bool readOk = reader.read(u8ToPath(currentModelPath_));
        const auto& st = reader.getParseStats();
        world_.loadDebug.attempted        = true;
        world_.loadDebug.filePath         = currentModelPath_;
        world_.loadDebug.success          = false;
        world_.loadDebug.failedAt         = st.failedAt;
        world_.loadDebug.vertCount        = st.vertCount;
        world_.loadDebug.idxCount         = st.idxCount;
        world_.loadDebug.texCount         = st.texCount;
        world_.loadDebug.matCount         = st.matCount;
        world_.loadDebug.boneCount        = st.boneCount;
        world_.loadDebug.morphCount       = st.morphCount;
        world_.loadDebug.streamPosAtFail  = st.lastBoneStreamPos;
        world_.loadDebug.boneSectionStart = st.boneSectionStart;
        if (!readOk) {
            std::fprintf(stderr,
                "[AnimationView] PMX read failed at \"%s\""
                " (boneSection=%" PRId64 ", bone58at=%" PRId64
                ", verts=%d idx=%d tex=%d mat=%d bones=%d morphs=%d): %s\n",
                world_.loadDebug.failedAt.c_str(),
                world_.loadDebug.boneSectionStart,
                world_.loadDebug.streamPosAtFail,
                world_.loadDebug.vertCount,
                world_.loadDebug.idxCount,
                world_.loadDebug.texCount,
                world_.loadDebug.matCount,
                world_.loadDebug.boneCount,
                world_.loadDebug.morphCount,
                currentModelPath_.c_str());
            return;
        }
    }

    GltfDocument        newDoc;
    MmdToGltfLoadStats   stats;
    const bool ok = MmdToGltfConverter::convert(
        u8ToPath(currentModelPath_), u8ToPath(currentVMDPath_), newDoc, {}, &stats);
    if (!ok) {
        world_.loadDebug.failedAt = "MmdToGltfConverter::convert()";
        std::fprintf(stderr, "[AnimationView] MMD->glTF conversion failed: %s\n", currentModelPath_.c_str());
        return;
    }

    world_.loadDebug.success = true;
    world_.document       = std::move(newDoc);
    world_.animationIndex = world_.document.animations.empty() ? -1 : 0;
    world_.skinIndex       = 0;
    world_.boneCount   = stats.boneCount;
    world_.ikCount     = stats.ikChainCount;
    world_.morphCount  = stats.morphTargetCount;
    world_.vertCount   = stats.vertexCount;
    world_.duration    = (world_.animationIndex >= 0)
        ? GltfAnimationEvaluator::duration(world_.document.animations[0], world_.document)
        : 0.f;
    world_.currentTime = 0.f;
    world_.playing     = false;
    world_.dirty       = true;

    sceneRenderer_.loadDocument(world_.document);

    std::fprintf(stdout,
        "[AnimationView] Loaded PMX: %s (%d bones, %d verts, %d IK, %d morphs)%s\n",
        currentModelPath_.c_str(), world_.boneCount, world_.vertCount, world_.ikCount, world_.morphCount,
        currentVMDPath_.empty() ? "" : (" + VMD: " + currentVMDPath_).c_str());
}

void AnimationViewApp::setupWindowCallbacks()
{
    auto& win = getWindow();
    win.onMouseButton = [this](int button, int action, int) {
        if (button == 0) sceneRenderer_.handleMouseButton(action == 1);
    };
    win.onCursorPos = [this](double x, double y) {
        sceneRenderer_.handleMouseMove(x, y);
    };
    win.onScroll = [this](double, double dy) {
        sceneRenderer_.handleScroll(dy);
    };
}
