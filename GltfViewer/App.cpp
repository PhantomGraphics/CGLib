#include "App.h"
#include "../GltfRenderer/Gltf/GltfReader.h"
#include "../GltfRenderer/Gltf/GltfBounds.h"
#include "../GltfRenderer/Gltf/GltfAnimationEvaluator.h"
#include "../GltfRenderer/Gltf/GltfMorphApply.h"
#include "../GltfRenderer/Gltf/ObjToGltfConverter.h"
#include "../GltfRenderer/Gltf/StlToGltfConverter.h"
#include "../GltfRenderer/Vrm/VrmReader.h"
#include "../File/File/OBJFileReader.h"
#include "../File/File/STLFileReader.h"
#include "../../CGLib/VulkanGraphics/VulkanSPVResolver.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
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
    if (!gltfPath.empty()) {
        if (!loadDocumentForPath(gltfPath, doc_, vrm_)) {
            fprintf(stderr, "GltfViewer: failed to load '%s'\n", gltfPath.string().c_str());
        }
        applySkinBindPose();
        renderer_.setDocument(doc_);
    }
    dispatcher_.setDocument(&doc_);
    dispatcher_.setRenderer(&renderer_);
    dispatcher_.setApp(this);
    //console_.setDispatcher(&dispatcher_);
    scenarioBrowser_.setHost(this);
    scenarioBrowser_.setDefaultFolder("scenarios");

    panel_.setRenderer(&renderer_);
    panel_.setFilePath(gltfPath);
    panel_.setOnFileOpen([this](const std::filesystem::path& p) {
        pendingPath_ = p;
    });
    panel_.setVrmState(&vrm_);
    panel_.setOnVrmExpressionChanged([this](int index, float weight) {
        setVrmExpressionWeight(index, weight);
    });
    add(&renderer_);
    add(&panel_);
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
}

void App::createEnvCubemap() {
    auto& ctx  = getContext();
    auto& pool = getCommandPool();
    VkDevice dev = ctx.getDevice();

    constexpr VkFormat    fmt     = VK_FORMAT_R32G32B32A32_SFLOAT;
    constexpr uint32_t    sz      = 1;
    constexpr uint32_t    faces   = 6;
    constexpr VkDeviceSize pixSize = sz * sz * 4 * sizeof(float);
    constexpr VkDeviceSize total   = pixSize * faces;

    // Cube image
    {
        VkImageCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        ci.imageType     = VK_IMAGE_TYPE_2D;
        ci.format        = fmt;
        ci.extent        = { sz, sz, 1 };
        ci.mipLevels     = 1;
        ci.arrayLayers   = faces;
        ci.samples       = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ci.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkCreateImage(dev, &ci, nullptr, &envImage_);

        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(dev, envImage_, &mr);
        auto memType = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        assert(memType.has_value());

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = memType.value_or(0);
        vkAllocateMemory(dev, &ai, nullptr, &envMem_);
        vkBindImageMemory(dev, envImage_, envMem_, 0);
    }

    // Staging buffer — dim sky-blue tint x 6 faces. This is a flat placeholder (no real
    // HDRI/skybox yet), used for both diffuse and specular IBL -- for a constant-color
    // environment the precomputed irradiance/prefiltered maps both converge to ~this same
    // color (see GltfIBLPrecomputer's irradiance.frag/prefilter.frag), so a bright value
    // here floods every material's ambient term and washes out all texture detail. Keep it
    // dim enough that it reads as a subtle fill light, not a dominant light source.
    const float skyColor[4] = { 0.05f, 0.07f, 0.10f, 1.0f };
    VkBuffer stageBuf; VkDeviceMemory stageMem;
    {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = total;
        bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(dev, &bi, nullptr, &stageBuf);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(dev, stageBuf, &mr);
        auto memType = ctx.findMemoryType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        assert(memType.has_value());

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = memType.value_or(0);
        vkAllocateMemory(dev, &ai, nullptr, &stageMem);
        vkBindBufferMemory(dev, stageBuf, stageMem, 0);

        void* mapped;
        vkMapMemory(dev, stageMem, 0, total, 0, &mapped);
        for (uint32_t f = 0; f < faces; ++f)
            std::memcpy(reinterpret_cast<float*>(mapped) + f * 4, skyColor, sizeof(skyColor));
        vkUnmapMemory(dev, stageMem);
    }

    // UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY
    {
        VkCommandBuffer cmd = pool.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = envImage_;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, faces };
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        for (uint32_t f = 0; f < faces; ++f) {
            VkBufferImageCopy region{};
            region.bufferOffset                    = f * pixSize;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = f;
            region.imageSubresource.layerCount     = 1;
            region.imageExtent                     = { sz, sz, 1 };
            vkCmdCopyBufferToImage(cmd, stageBuf, envImage_,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        pool.endSingleTimeCommands(cmd);
    }

    vkDestroyBuffer(dev, stageBuf, nullptr);
    vkFreeMemory(dev, stageMem, nullptr);

    // Image view
    {
        VkImageViewCreateInfo vci{};
        vci.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image            = envImage_;
        vci.viewType         = VK_IMAGE_VIEW_TYPE_CUBE;
        vci.format           = fmt;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
        vkCreateImageView(dev, &vci, nullptr, &envView_);
    }

    // Sampler
    {
        VkSamplerCreateInfo si{};
        si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter    = VK_FILTER_LINEAR;
        si.minFilter    = VK_FILTER_LINEAR;
        si.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.maxLod       = 1.0f;
        vkCreateSampler(dev, &si, nullptr, &envSampler_);
    }
}

void App::destroyEnvCubemap() {
    VkDevice dev = getDevice();
    if (envSampler_) { vkDestroySampler(dev, envSampler_, nullptr);   envSampler_ = VK_NULL_HANDLE; }
    if (envView_)    { vkDestroyImageView(dev, envView_, nullptr);     envView_    = VK_NULL_HANDLE; }
    if (envImage_)   { vkDestroyImage(dev, envImage_, nullptr);        envImage_   = VK_NULL_HANDLE; }
    if (envMem_)     { vkFreeMemory(dev, envMem_, nullptr);            envMem_     = VK_NULL_HANDLE; }
}

void App::onInit() {
    applyShaders();
    createEnvCubemap();
    renderer_.setEnvironment(envView_, envSampler_);
    ::VKG::VkAppBase::onInit();
    renderer_.setExtent(getExtent());
    // Default off: the environment cubemap above is a flat placeholder color, not a real
    // HDRI/skybox (see createEnvCubemap()). Even heavily dimmed, full diffuse+specular IBL
    // against that flat color still visibly overwhelms every material's own base color/texture
    // detail (confirmed by A/B screenshot comparison across DamagedHelmet/Corset/Duck/Avocado/
    // AntiqueCamera -- all washed toward a uniform pale tint with IBL on). Leave it available as
    // an opt-in (panel checkbox / SetUseIBL scenario command) for once a real environment map is
    // wired up, but don't make a visibly-broken default.
    renderer_.setUseIBL(false);
    frameCameraToDocument();
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
    return true;
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
}

void App::onCleanup() {
    ::VKG::VkAppBase::onCleanup();
    destroyEnvCubemap();
}

void App::onImGui() {
    ::VKG::VkAppBase::onImGui();
    drawMainMenuBar();
}

void App::drawMainMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...")) {
            Phantom::UI::FileOpenDialog dlg("Open glTF");
            dlg.addFilter("*.gltf");
            dlg.addFilter("*.glb");
            dlg.addFilter("*.vrm");
            dlg.addFilter("*.obj");
            dlg.addFilter("*.stl");
            dlg.show();
            const auto path = dlg.getFilePath();
            if (!path.empty()) pendingPath_ = path;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) getWindow().close();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Reset Camera")) frameCameraToDocument();

        bool useIBL = renderer_.getUseIBL() != 0;
        if (ImGui::MenuItem("Use IBL", nullptr, useIBL)) renderer_.setUseIBL(!useIBL);

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
