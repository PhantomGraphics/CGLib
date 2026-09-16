#include "App.h"
#include "../GltfRenderer/Gltf/GltfReader.h"
#include "../GltfRenderer/Gltf/GltfBounds.h"
#include "../GltfRenderer/Gltf/GltfAnimationEvaluator.h"
#include "../GltfRenderer/Gltf/GltfMorphApply.h"
#include "../GltfRenderer/Gltf/ObjToGltfConverter.h"
#include "../GltfRenderer/Gltf/StlToGltfConverter.h"
#include "../GltfRenderer/Vrm/VrmReader.h"
#include "../GltfRenderer/Phmat/PhmatCompiler.h"
#include "../File/File/OBJFileReader.h"
#include "../File/File/STLFileReader.h"
#include "../../CGLib/VulkanGraphics/VulkanSPVResolver.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../CGLib/ThirdParty/tinyfiledialogs/tinyfiledialogs.h"
#include "../../CGLib/UIWidgets/FileOpenDialog.h"
#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>

using namespace Phantom::Gltf;

namespace {

// Keep the viewer useful even when it is launched without a model.  The default
// cube is represented by the same small in-memory glTF document used by loaded
// assets, so all renderer, bounds, and scene-graph code follows one path.
GltfDocument makeDefaultCubeDocument() {
    GltfDocument doc;

    const std::vector<glm::vec3> positions = {
        {-1.f, -1.f,  1.f}, { 1.f, -1.f,  1.f}, { 1.f,  1.f,  1.f}, {-1.f,  1.f,  1.f}, // front
        { 1.f, -1.f, -1.f}, {-1.f, -1.f, -1.f}, {-1.f,  1.f, -1.f}, { 1.f,  1.f, -1.f}, // back
        {-1.f, -1.f, -1.f}, {-1.f, -1.f,  1.f}, {-1.f,  1.f,  1.f}, {-1.f,  1.f, -1.f}, // left
        { 1.f, -1.f,  1.f}, { 1.f, -1.f, -1.f}, { 1.f,  1.f, -1.f}, { 1.f,  1.f,  1.f}, // right
        {-1.f,  1.f,  1.f}, { 1.f,  1.f,  1.f}, { 1.f,  1.f, -1.f}, {-1.f,  1.f, -1.f}, // top
        {-1.f, -1.f, -1.f}, { 1.f, -1.f, -1.f}, { 1.f, -1.f,  1.f}, {-1.f, -1.f,  1.f}, // bottom
    };
    const std::vector<glm::vec3> normals = {
        { 0.f,  0.f,  1.f}, { 0.f,  0.f, -1.f}, {-1.f,  0.f,  0.f}, { 1.f,  0.f,  0.f},
        { 0.f,  1.f,  0.f}, { 0.f, -1.f,  0.f},
    };
    const std::vector<glm::vec2> faceUv = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
    std::vector<glm::vec2> texCoords;
    texCoords.reserve(24);
    for (int face = 0; face < 6; ++face)
        texCoords.insert(texCoords.end(), faceUv.begin(), faceUv.end());

    std::vector<glm::vec3> expandedNormals;
    expandedNormals.reserve(24);
    for (const auto& normal : normals)
        for (int vertex = 0; vertex < 4; ++vertex)
            expandedNormals.push_back(normal);

    const std::vector<uint32_t> indices = {
         0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8, 12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
    };

    auto addAccessor = [&doc](const void* data, size_t byteSize, size_t count,
                              GltfAccessorType type, GltfComponentType componentType) {
        const int bufferIndex = static_cast<int>(doc.buffers.size());
        GltfBuffer buffer;
        buffer.data.resize(byteSize);
        std::memcpy(buffer.data.data(), data, byteSize);
        doc.buffers.push_back(std::move(buffer));

        const int viewIndex = static_cast<int>(doc.bufferViews.size());
        doc.bufferViews.push_back({bufferIndex, 0, byteSize, 0, 0});
        doc.accessors.push_back({viewIndex, 0, componentType, type, count, false});
        return static_cast<int>(doc.accessors.size() - 1);
    };

    GltfPrimitive primitive;
    primitive.positionAccessor = addAccessor(positions.data(), positions.size() * sizeof(glm::vec3),
                                            positions.size(), GltfAccessorType::Vec3,
                                            GltfComponentType::Float);
    primitive.normalAccessor = addAccessor(expandedNormals.data(), expandedNormals.size() * sizeof(glm::vec3),
                                           expandedNormals.size(), GltfAccessorType::Vec3,
                                           GltfComponentType::Float);
    primitive.texCoord0Accessor = addAccessor(texCoords.data(), texCoords.size() * sizeof(glm::vec2),
                                              texCoords.size(), GltfAccessorType::Vec2,
                                              GltfComponentType::Float);
    primitive.indicesAccessor = addAccessor(indices.data(), indices.size() * sizeof(uint32_t),
                                            indices.size(), GltfAccessorType::Scalar,
                                            GltfComponentType::UnsignedInt);

    doc.meshes.push_back({"Default Cube", {primitive}, {}});
    doc.nodes.push_back({"Default Cube", 0});
    doc.scenes.push_back({"Default Scene", {0}});
    doc.defaultScene = 0;
    return doc;
}

// .obj/.stl are converted to a GltfDocument on the fly (Phantom::Gltf::ObjToGltfConverter/
// StlToGltfConverter); anything else still goes through
// GltfReader::load() as a real .gltf/.glb file.
std::string lowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::optional<GltfDocument> loadAsGltfDocument(const std::filesystem::path& path) {
    const std::string ext = lowerExtension(path);
    if (ext == ".obj") {
        Phantom::File::OBJFileReader reader;
        if (!reader.read(path)) return std::nullopt;
        GltfDocument doc = ObjToGltfConverter::convert(reader.getOBJ());
        if (doc.meshes.empty()) return std::nullopt;
        return doc;
    }
    if (ext == ".stl") {
        Phantom::File::STLFileReader reader;
        const bool ok = Phantom::File::STLFileReader::isBinary(path)
            ? reader.readBinary(path)
            : reader.readAscii(path);
        if (!ok) return std::nullopt;
        GltfDocument doc = StlToGltfConverter::convert(reader.getSTL());
        if (doc.meshes.empty()) return std::nullopt;
        return doc;
    }
    return GltfReader::load(path);
}

} // namespace

App::App(const std::filesystem::path& gltfPath)
    : ::VKG::VkAppBase(1280, 720, gltfPath.empty()
          ? "glTF Viewer"
          : "glTF Viewer - " + gltfPath.filename().string())
{
    if (gltfPath.empty()) {
        doc_ = makeDefaultCubeDocument();
    } else {
        if (!loadDocumentForPath(gltfPath, doc_, vrm_)) {
            fprintf(stderr, "GltfViewer: failed to load '%s'\n", gltfPath.string().c_str());
        }
    }
    applySkinBindPose();
    renderer_.setDocument(doc_);
    dispatcher_.setDocument(&doc_);
    dispatcher_.setRenderer(&renderer_);
    dispatcher_.setApp(this);
    //console_.setDispatcher(&dispatcher_);
    scenarioBrowser_.setHost(this);
    scenarioBrowser_.setDefaultFolder("scenarios");

    panel_.setFilePath(gltfPath);
    panel_.setDocument(&doc_);
    panel_.setSelectedNode(&selectedNode_);
    sceneGraphPanel_.setDocument(&doc_);
    sceneGraphPanel_.setSelectedNode(&selectedNode_);
    viewPanel_.setRenderer(&renderer_);
    viewPanel_.setApp(this);
    panel_.setVrmState(&vrm_);
    panel_.setOnVrmExpressionChanged([this](int index, float weight) {
        setVrmExpressionWeight(index, weight);
    });
    add(&renderer_);
    add(&panel_);
    add(&sceneGraphPanel_);
    add(&viewPanel_);
    //add(&console_);
    add(&scenarioBrowser_);
}

bool App::loadScenario(const std::string& jsonPath) {
    return runner_.load(jsonPath);
}

void App::applyShaders() {
    GltfSceneRenderer::Shaders s;
    s.vertSpv            = ::VKG::loadSPVRepo("shaders/gltf.vert.spv");
    s.fragSpv            = ::VKG::loadSPVRepo("shaders/gltf.frag.spv");
    s.skyboxVertSpv      = ::VKG::loadSPVRepo("shaders/skybox.vert.spv");
    s.skyboxFragSpv      = ::VKG::loadSPVRepo("shaders/skybox.frag.spv");
    s.ibl.irradianceVert = ::VKG::loadSPVRepo("shaders/irradiance.vert.spv");
    s.ibl.irradianceFrag = ::VKG::loadSPVRepo("shaders/irradiance.frag.spv");
    s.ibl.prefilterVert  = ::VKG::loadSPVRepo("shaders/prefilter.vert.spv");
    s.ibl.prefilterFrag  = ::VKG::loadSPVRepo("shaders/prefilter.frag.spv");
    s.ibl.brdfVert       = ::VKG::loadSPVRepo("shaders/brdf_lut.vert.spv");
    s.ibl.brdfFrag       = ::VKG::loadSPVRepo("shaders/brdf_lut.frag.spv");
    renderer_.setShaders(std::move(s));

    // Real-HDRI loading (2026-09-15): shaders/equirect_to_cube.{vert,frag}, loaded once here and
    // copied into each loadEnvironmentHDR() call. See GltfEnvironmentCubemap::loadFromHDR()'s comment.
    equirectVertSpv_ = ::VKG::loadSPVRepo("shaders/equirect_to_cube.vert.spv");
    equirectFragSpv_ = ::VKG::loadSPVRepo("shaders/equirect_to_cube.frag.spv");
}

bool App::loadEnvironmentHDR(const std::string& path) {
    if (!envCubemap_.loadFromHDR(getContext(), getCommandPool(), path, equirectVertSpv_, equirectFragSpv_))
        return false;
    // Review R1 pattern (see CGApp/Universe/Rendering/Renderer.cpp's setShadowEnabled()'s
    // comment for the detailed rationale): setEnvironment() rewrites the global descriptor set
    // for every frame in flight, unconditionally -- called mid-frame (a dispatcher command),
    // that could rewrite a frame-in-flight's descriptor set the GPU is still executing against.
    // Block on full device idle first; this is a rare, user-triggered event, not a per-frame one.
    vkDeviceWaitIdle(getDevice());
    renderer_.setEnvironment(envCubemap_.getView(), envCubemap_.getSampler());
    return true;
}

void App::clearEnvironmentHDR() {
    if (!envCubemap_.resetToPlaceholder(getContext(), getCommandPool())) return;
    vkDeviceWaitIdle(getDevice()); // same hazard as loadEnvironmentHDR() above
    renderer_.setEnvironment(envCubemap_.getView(), envCubemap_.getSampler());
}

bool App::loadPhmatMaterial(int materialIndex, const std::string& path, std::string* outError) {
    // A per-viewer, cross-session cache directory: the same .phmat compiles to the same SPIR-V
    // regardless of which document/material it is applied to (see PhmatCompiler.h's comment on
    // why SPIR-V is a cache, not the source of truth), so there is no reason to scope this by
    // materialIndex or the current document.
    const std::string cacheDir = (std::filesystem::temp_directory_path() / "phantom_phmat_cache").string();

    Phantom::Gltf::Phmat::PhmatLoadResult result = Phantom::Gltf::Phmat::loadPhmatMaterial(path, cacheDir);
    if (!result.success) {
        if (outError) *outError = result.diagnostics.empty() ? "unknown .phmat error" : result.diagnostics.front().message;
        return false;
    }

    // Review R1 pattern (see loadEnvironmentHDR() above): block on full device idle before
    // swapping a pipeline a frame in flight might still be drawing through.
    vkDeviceWaitIdle(getDevice());
    std::string pipelineError;
    if (!renderer_.setMaterialShaderOverride(materialIndex, result.fragSpirv, &pipelineError)) {
        if (outError) *outError = pipelineError;
        return false;
    }
    return true;
}

void App::clearPhmatMaterial(int materialIndex) {
    vkDeviceWaitIdle(getDevice()); // same hazard as loadPhmatMaterial() above
    renderer_.clearMaterialShaderOverride(materialIndex);
}

void App::onInit() {
    applyShaders();
    auto& ctx  = getContext();
    auto& pool = getCommandPool();
    envCubemap_.create(ctx, pool);
    renderer_.setEnvironment(envCubemap_.getView(), envCubemap_.getSampler());
    ::VKG::VkAppBase::onInit();
    renderer_.setExtent(getExtent());
    // Default off: the environment cubemap above is a flat placeholder color, not a real
    // HDRI/skybox (see GltfEnvironmentCubemap::create()). Even heavily dimmed, full
    // diffuse+specular IBL against that flat color still visibly overwhelms every material's own
    // base color/texture detail (confirmed by A/B screenshot comparison across DamagedHelmet/
    // Corset/Duck/Avocado/AntiqueCamera -- all washed toward a uniform pale tint with IBL on).
    // Leave it available as an opt-in (panel checkbox / SetUseIBL scenario command) for once a
    // real environment map is loaded (GltfEnvironmentCubemap::loadFromHDR()), but don't make a
    // visibly-broken default.
    renderer_.setUseIBL(false);
    frameCameraToDocument();
    captureAssetCamera();
    setupCallbacks();
}

void App::onUpdate(uint32_t frameIndex) {
    if (pendingPath_) {
        loadFile(*pendingPath_);
        pendingPath_.reset();
    }

    dispatcher_.processQueue();

    if (auto p = dispatcher_.takePendingLoad()) {
        bool ok = loadFile(*p);
        dispatcher_.signalLoaded(ok, ok ? "" : "load failed");
    }

    if (screenshotPending_ && isScreenshotDone()) {
        dispatcher_.signalScreenshotDone(true, screenshotPendingPath_);
        screenshotPending_ = false;
    }
    if (auto p = dispatcher_.takePendingScreenshot()) {
        std::filesystem::create_directories(p->parent_path());
        screenshotPendingPath_ = p->string();
        screenshotPending_     = true;
        requestScreenshot(screenshotPendingPath_);
    }

    if (runner_.isActive()) {
        auto responses = dispatcher_.collectResponses();
        if (runner_.tick(dispatcher_, responses)) {
            if (runner_.hasFailed()) {
                fprintf(stderr, "[Scenario] FAILED: %s\n", runner_.failMessage().c_str());
                exitCode_ = 1;
            } else {
                fprintf(stdout, "[Scenario] PASSED (%zu steps)\n", runner_.stepCount());
                exitCode_ = 0;
            }
            if (exitOnComplete_) getWindow().close();
        }
    } else {
        //console_.addResponses(dispatcher_.collectResponses());
    }

    ::VKG::VkAppBase::onUpdate(frameIndex);
}

void App::frameCameraToDocument() {
    GltfAabb aabb = computeGltfBounds(doc_);
    if (!aabb.valid) return;

    float radius = glm::length(aabb.halfExtents());
    if (radius < 1e-4f) radius = 1e-4f;

    *renderer_.camTargetPtr() = aabb.center();
    *renderer_.camDistPtr()   = radius * 2.2f;
}

bool App::loadFile(const std::filesystem::path& path) {
    vkDeviceWaitIdle(getDevice());
    renderer_.onCleanup(getDevice());

    GltfDocument newDoc;
    VrmViewState newVrm;
    if (!loadDocumentForPath(path, newDoc, newVrm)) {
        fprintf(stderr, "GltfViewer: failed to load '%s'\n", path.string().c_str());
        return false;
    }

    doc_ = std::move(newDoc);
    vrm_ = std::move(newVrm);
    dispatcher_.setDocument(&doc_);
    renderer_.setDocument(doc_);
    panel_.setFilePath(path);
    applyShaders();
    renderer_.onInit(getContext(), getCommandPool(), getRenderPass(), MAX_FRAMES_IN_FLIGHT);
    applySkinBindPose();
    frameCameraToDocument();
    captureAssetCamera();
    return true;
}

void App::captureAssetCamera() {
    // Camera-as-scene-component: capture doc_'s first camera, if any, but stay on the live orbit
    // camera until the user opts in (SetUseAssetCamera:1) -- see App.h's comment.
    const auto lc = Phantom::Gltf::collectGltfLightsAndCameras(doc_);
    hasAssetCamera_ = !lc.cameras.empty();
    assetCameraInst_ = hasAssetCamera_ ? lc.cameras.front() : Phantom::Gltf::GltfCameraInstance{};
    useAssetCamera_ = false;
    renderer_.clearCameraOverride();
}

void App::pushAssetCameraOverride() {
    if (!hasAssetCamera_) return;
    const VkExtent2D ext = renderer_.getExtent();
    const float aspect = ext.height > 0
        ? static_cast<float>(ext.width) / static_cast<float>(ext.height) : 1.f;
    const auto vp = Phantom::Gltf::computeCameraViewProj(doc_, assetCameraInst_, aspect);
    renderer_.setCamera(vp.view, vp.proj, vp.eye);
}

void App::setUseAssetCamera(bool use) {
    useAssetCamera_ = use && hasAssetCamera_;
    if (useAssetCamera_)
        pushAssetCameraOverride();
    else
        renderer_.clearCameraOverride(); // back to the orbit camera GltfSceneRenderer maintains
}

bool App::loadDocumentForPath(const std::filesystem::path& path,
                                         GltfDocument& outDoc, VrmViewState& outVrm) {
    outVrm = VrmViewState{};

    if (lowerExtension(path) == ".vrm") {
        auto vrmDoc = VrmReader::load(path);
        if (!vrmDoc) return false;
        outDoc = std::move(vrmDoc->gltf);
        outVrm.active         = true;
        outVrm.specVersion    = vrmDoc->specVersion;
        outVrm.humanoid       = std::move(vrmDoc->humanoid);
        outVrm.expressions    = std::move(vrmDoc->expressions);
        outVrm.expressionWeights.assign(outVrm.expressions.size(), 0.0f);
        outVrm.meta           = std::move(vrmDoc->meta);
        return true;
    }

    auto doc = loadAsGltfDocument(path);
    if (!doc) return false;
    outDoc = std::move(*doc);
    return true;
}

void App::applySkinBindPose() {
    if (doc_.skins.empty()) return;
    renderer_.updateSkinMatrices(GltfAnimationEvaluator::evaluateSkin(doc_, -1, 0, 0.f));
}

void App::setVrmExpressionWeight(int index, float weight) {
    if (index < 0 || index >= static_cast<int>(vrm_.expressionWeights.size())) return;
    vrm_.expressionWeights[index] = weight;
    applyVrmExpressionWeights();
}

void App::applyVrmExpressionWeights() {
    // node -> targetIndex -> accumulated weight. Seed every (node,targetIndex) any expression can
    // ever affect with 0 first, so a target whose driving expression weight just dropped back to
    // 0 gets explicitly reset instead of staying stuck at its last nonzero blend.
    std::map<int, std::map<int, float>> perNodeTargetWeights;
    for (const auto& expr : vrm_.expressions)
        for (const auto& bind : expr.binds)
            perNodeTargetWeights[bind.node].emplace(bind.targetIndex, 0.0f);

    for (size_t i = 0; i < vrm_.expressions.size(); ++i) {
        const float w = (i < vrm_.expressionWeights.size()) ? vrm_.expressionWeights[i] : 0.0f;
        if (w <= 0.0f) continue;
        for (const auto& bind : vrm_.expressions[i].binds) {
            float& acc = perNodeTargetWeights[bind.node][bind.targetIndex];
            acc = std::clamp(acc + w * bind.weight, 0.0f, 1.0f);
        }
    }

    for (const auto& [nodeIndex, targetWeights] : perNodeTargetWeights) {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc_.nodes.size())) continue;
        const int meshIndex = doc_.nodes[nodeIndex].meshIndex;
        if (meshIndex < 0 || meshIndex >= static_cast<int>(doc_.meshes.size())) continue;

        const GltfMesh& mesh = doc_.meshes[meshIndex];
        for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex) {
            const GltfPrimitive& prim = mesh.primitives[primIndex];
            if (prim.targets.empty()) continue;

            std::vector<float> weights(prim.targets.size(), 0.0f);
            for (const auto& [targetIndex, w] : targetWeights) {
                if (targetIndex >= 0 && targetIndex < static_cast<int>(weights.size()))
                    weights[targetIndex] = w;
            }

            const auto positions = applyMorphs(doc_, prim, weights);
            renderer_.updateMorphedPositions(meshIndex, static_cast<int>(primIndex), positions);
        }
    }
}

void App::onSwapChainCreated() {
    renderer_.setExtent(getExtent());
    // An active asset camera's projection was computed from the aspect ratio at the moment it
    // was (re-)applied, not re-derived every frame the way the orbit camera's is (GltfSceneRenderer::
    // onUpdate()) -- re-push it here or an aspectRatio-less (glTF spec default) camera stays
    // stretched/squashed at the old ratio (same hazard as Universe's review R3 fix).
    if (useAssetCamera_) pushAssetCameraOverride();
}

void App::onCleanup() {
    ::VKG::VkAppBase::onCleanup();
    envCubemap_.destroy(getDevice());
}

void App::onImGui() {
    ::VKG::VkAppBase::onImGui();
    drawMainMenuBar();
}

void App::drawMainMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...")) {
#ifdef _WIN32
            const wchar_t* filters[] = { L"*.gltf", L"*.glb", L"*.vrm", L"*.obj", L"*.stl" };
            const wchar_t* fileName = tinyfd_openFileDialogW(
                L"Open glTF", L"", 5, filters, nullptr, 0);
            if (fileName != nullptr && *fileName != L'\0')
                pendingPath_ = std::filesystem::path(fileName);
#else
            Phantom::UI::FileOpenDialog dlg("Open glTF");
            dlg.addFilter("*.gltf");
            dlg.addFilter("*.glb");
            dlg.addFilter("*.vrm");
            dlg.addFilter("*.obj");
            dlg.addFilter("*.stl");
            dlg.show();
            const auto fileName = dlg.getFileName();
            if (!fileName.empty()) pendingPath_ = std::filesystem::path(fileName);
#endif
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) getWindow().close();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        bool viewVisible = viewPanel_.isVisible();
        if (ImGui::MenuItem("View Settings", nullptr, viewVisible))
            viewPanel_.setVisible(!viewVisible);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Camera")) frameCameraToDocument();

        bool useIBL = renderer_.getUseIBL() != 0;
        if (ImGui::MenuItem("Use IBL", nullptr, useIBL)) renderer_.setUseIBL(!useIBL);

        if (ImGui::MenuItem("Use Asset Camera", nullptr, useAssetCamera_, hasAssetCamera_))
            setUseAssetCamera(!useAssetCamera_);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        bool controlVisible = panel_.isVisible();
        if (ImGui::MenuItem("Control", nullptr, controlVisible))
            panel_.setVisible(!controlVisible);

        bool viewVisible = viewPanel_.isVisible();
        if (ImGui::MenuItem("View", nullptr, viewVisible))
            viewPanel_.setVisible(!viewVisible);

        bool sceneGraphVisible = sceneGraphPanel_.isVisible();
        if (ImGui::MenuItem("Scene Graph", nullptr, sceneGraphVisible))
            sceneGraphPanel_.setVisible(!sceneGraphVisible);

        bool scenarioVisible = scenarioBrowser_.isVisible();
        if (ImGui::MenuItem("Scenario Browser", nullptr, scenarioVisible))
            scenarioBrowser_.setVisible(!scenarioVisible);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void App::setupCallbacks() {
    auto& win = getWindow();
    win.onMouseButton = [this](int btn, int action, int) {
        if (btn == 0) renderer_.handleMouseButton(action == 1);
    };
    win.onCursorPos = [this](double x, double y) {
        renderer_.handleMouseMove(x, y);
    };
    win.onScroll = [this](double, double dy) {
        renderer_.handleScroll(dy);
    };
}
