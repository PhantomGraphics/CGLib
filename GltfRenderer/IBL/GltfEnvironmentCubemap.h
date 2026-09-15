#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Gltf {

// Owns a cube environment map fed to GltfSceneRenderer::setEnvironment() so its useIBL sampling
// path has something to read. Two states: a cheap flat placeholder tint (create()) for apps that
// haven't wired up a real environment, and a real one baked from an equirectangular .hdr panorama
// (loadFromHDR(), via GltfIBLPrecomputer::computeEnvironmentCube()).
//
// Shared infrastructure (2026-09-15, moved here from an app-local class): CGLib/GltfViewer and
// CGApp/Universe each used to carry their own copy of this exact placeholder-cubemap boilerplate
// (see git history's App.cpp::createEnvCubemap()/destroyEnvCubemap()) -- a duplication that made
// the real-HDRI loading work landing in one app not automatically benefit the other, despite both
// being pure glTF/IBL rendering concerns with no dependency on either app's own domain (Universe's
// animation/physics, GltfViewer's plain-viewer UI). Consumers own the VulkanContext/CommandPool
// lifetime and simply hold one of these plus call create()/loadFromHDR()/destroy() at the same
// points they already call GltfSceneRenderer::setEnvironment().
//
// The placeholder itself is a deliberately dim sky tint (not pure black): a bright flat
// environment floods every material's ambient term and washes out texture detail (confirmed by
// A/B screenshot comparison across several glTF samples, both in GltfViewer originally and again
// in Universe).
class GltfEnvironmentCubemap {
public:
    GltfEnvironmentCubemap() = default;
    GltfEnvironmentCubemap(const GltfEnvironmentCubemap&) = delete;
    GltfEnvironmentCubemap& operator=(const GltfEnvironmentCubemap&) = delete;

    bool create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool);
    void destroy(VkDevice device);

    // Loads `path` (an equirectangular .hdr panorama) and replaces the current cubemap (whatever
    // it was -- the flat placeholder from create(), or a previously loaded HDRI) with a real
    // environment cube baked from it. `equirectVert`/`equirectFrag` are the consuming app's own
    // compiled copy of shaders/equirect_to_cube.{vert,frag}'s SPIR-V (loaded once by the caller
    // and passed by value each call -- GltfIBLPrecomputer::computeEnvironmentCube() consumes
    // them). On failure (file not found/unreadable, GPU step failure) the current cubemap is left
    // untouched and this returns false; the caller decides whether to report an error or fall
    // back via resetToPlaceholder().
    bool loadFromHDR(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                     const std::string& path,
                     std::vector<uint32_t> equirectVert, std::vector<uint32_t> equirectFrag,
                     uint32_t cubeSize = 512);

    // Discards whatever cubemap is currently active and rebuilds the flat placeholder tint (same
    // as a fresh create()). No-op-safe to call even if already showing the placeholder.
    bool resetToPlaceholder(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool);

    VkImageView getView()    const { return view_; }
    VkSampler   getSampler() const { return sampler_; }
    bool        isValid()    const { return view_ != VK_NULL_HANDLE; }

    // True once loadFromHDR() has successfully replaced the placeholder with a real panorama.
    bool        isRealHDR()  const { return isRealHDR_; }
    // Path last passed to a successful loadFromHDR(); empty while showing the placeholder.
    const std::string& hdrPath() const { return hdrPath_; }

private:
    VkImage        image_   = VK_NULL_HANDLE;
    VkDeviceMemory memory_  = VK_NULL_HANDLE;
    VkImageView    view_    = VK_NULL_HANDLE;
    VkSampler      sampler_ = VK_NULL_HANDLE;

    bool        isRealHDR_ = false;
    std::string hdrPath_;
};

} // namespace Phantom::Gltf
