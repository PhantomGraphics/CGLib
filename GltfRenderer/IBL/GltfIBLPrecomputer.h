#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace Phantom::VKG {
class VulkanContext;
class VulkanCommandPool;
}

namespace Phantom::Gltf {

// Computes IBL textures (irradiance map, prefiltered env map, BRDF LUT)
// from a cubemap environment texture. All computation is done on the GPU
// using one-shot render passes at startup.
class GltfIBLPrecomputer {
public:
    struct Result {
        VkImage     irradianceImage   = VK_NULL_HANDLE;
        VkDeviceMemory irradianceMem  = VK_NULL_HANDLE;
        VkImageView irradianceView    = VK_NULL_HANDLE;
        VkSampler   irradianceSampler = VK_NULL_HANDLE;

        VkImage     prefilterImage    = VK_NULL_HANDLE;
        VkDeviceMemory prefilterMem   = VK_NULL_HANDLE;
        VkImageView prefilterView     = VK_NULL_HANDLE;
        VkSampler   prefilterSampler  = VK_NULL_HANDLE;

        VkImage     brdfLUTImage      = VK_NULL_HANDLE;
        VkDeviceMemory brdfLUTMem     = VK_NULL_HANDLE;
        VkImageView brdfLUTView       = VK_NULL_HANDLE;
        VkSampler   brdfLUTSampler    = VK_NULL_HANDLE;

        bool isValid() const { return irradianceView != VK_NULL_HANDLE; }
    };

    // Output of computeEnvironmentCube(): a plain environment cubemap (no irradiance/prefilter/
    // BRDF convolution -- that's what Result above is for), built by rendering all 6 faces of a
    // direction-to-equirectangular-UV lookup against a 2D panorama texture. Feed this cube's
    // view/sampler into compute() above (as envCubeView/envSampler) to get real IBL out of a
    // real HDRI, instead of a flat placeholder tint cubemap.
    struct EnvCubeResult {
        VkImage        image   = VK_NULL_HANDLE;
        VkDeviceMemory mem     = VK_NULL_HANDLE;
        VkImageView    view    = VK_NULL_HANDLE;
        VkSampler      sampler = VK_NULL_HANDLE;

        bool isValid() const { return view != VK_NULL_HANDLE; }
    };

    struct Shaders {
        std::vector<uint32_t> irradianceVert, irradianceFrag;
        std::vector<uint32_t> prefilterVert,  prefilterFrag;
        std::vector<uint32_t> brdfVert,       brdfFrag;
    };

    // Precompute all IBL textures. envCubeView must be a valid CUBE image view.
    // Returns nullopt if shaders are empty or computation cannot proceed.
    std::optional<Result> compute(const Phantom::VKG::VulkanContext& ctx,
                                  const Phantom::VKG::VulkanCommandPool& pool,
                                  VkImageView envCubeView,
                                  VkSampler   envSampler,
                                  Shaders shaders);

    void destroy(VkDevice device, Result& result);

    // Converts an equirectangular 2D environment texture (a typical .hdr panorama) into a cube
    // environment map, by rendering each of the 6 cube faces with a direction -> equirect-UV
    // lookup fragment shader (equirectVert/equirectFrag; the vertex shader can be identical to
    // Shaders::irradianceVert -- same cube-face view/proj push constants). Returns nullopt if
    // either shader is empty or any GPU step fails. cubeSize is the resolution of each face.
    std::optional<EnvCubeResult> computeEnvironmentCube(const Phantom::VKG::VulkanContext& ctx,
                                                        const Phantom::VKG::VulkanCommandPool& pool,
                                                        VkImageView equirectView,
                                                        VkSampler   equirectSampler,
                                                        uint32_t    cubeSize,
                                                        std::vector<uint32_t> equirectVert,
                                                        std::vector<uint32_t> equirectFrag);

    void destroy(VkDevice device, EnvCubeResult& result);

private:
    // ---- Cube geometry ----
    bool createCubeBuffers(const Phantom::VKG::VulkanContext& ctx,
                           const Phantom::VKG::VulkanCommandPool& pool);
    void destroyCubeBuffers(VkDevice device);

    VkBuffer       cubeVB_    = VK_NULL_HANDLE;
    VkDeviceMemory cubeVBMem_ = VK_NULL_HANDLE;
    VkBuffer       cubeIB_    = VK_NULL_HANDLE;
    VkDeviceMemory cubeIBMem_ = VK_NULL_HANDLE;

    // ---- Generic cube-face rendering ----
    struct CubeFacePass {
        VkRenderPass  renderPass  = VK_NULL_HANDLE;
        VkPipeline    pipeline    = VK_NULL_HANDLE;
        VkPipelineLayout layout   = VK_NULL_HANDLE;
        VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
        VkDescriptorPool pool_    = VK_NULL_HANDLE;
        VkDescriptorSet  set      = VK_NULL_HANDLE;
    };

    CubeFacePass createCubeFacePass(const Phantom::VKG::VulkanContext& ctx,
                                    VkFormat targetFmt,
                                    const std::vector<uint32_t>& vertSpv,
                                    const std::vector<uint32_t>& fragSpv,
                                    bool hasCubeSampler,
                                    uint32_t pushSize);
    void destroyCubeFacePass(VkDevice dev, CubeFacePass& p);

    void renderCubeFaces(const Phantom::VKG::VulkanContext& ctx,
                         const Phantom::VKG::VulkanCommandPool& pool,
                         const CubeFacePass& pass,
                         VkImage targetImage,
                         uint32_t faceSize,
                         uint32_t mipLevel,
                         float roughness);

    // ---- Irradiance ----
    bool computeIrradiance(const Phantom::VKG::VulkanContext& ctx,
                           const Phantom::VKG::VulkanCommandPool& pool,
                           VkImageView envView,
                           VkSampler   envSampler,
                           Result& res);

    // ---- Prefiltered env map ----
    bool computePrefilter(const Phantom::VKG::VulkanContext& ctx,
                          const Phantom::VKG::VulkanCommandPool& pool,
                          VkImageView envView,
                          VkSampler   envSampler,
                          Result& res);

    // ---- BRDF LUT ----
    bool computeBRDFLUT(const Phantom::VKG::VulkanContext& ctx,
                        const Phantom::VKG::VulkanCommandPool& pool,
                        Result& res);

    // ---- Image helpers ----
    bool createCubeImage(const Phantom::VKG::VulkanContext& ctx,
                         uint32_t size, uint32_t mipLevels, VkFormat fmt,
                         VkImage& img, VkDeviceMemory& mem);
    VkImageView createCubeView(VkDevice dev, VkImage img, VkFormat fmt, uint32_t mipLevels);
    VkImageView createCubeFaceView(VkDevice dev, VkImage img, VkFormat fmt,
                                   uint32_t face, uint32_t mip);
    VkSampler   createLinearSampler(VkDevice dev, uint32_t mipLevels = 1);

    VkShaderModule createShaderModule(VkDevice dev, const std::vector<uint32_t>& spv);

    // Shader SPV data held during compute() invocation
    std::vector<uint32_t> irradianceVertSpv_, irradianceFragSpv_;
    std::vector<uint32_t> prefilterVertSpv_,  prefilterFragSpv_;
    std::vector<uint32_t> brdfVertSpv_,       brdfFragSpv_;

    static const uint32_t kIrradianceSize = 32;
    static const uint32_t kPrefilterSize  = 128;
    static const uint32_t kPrefilterMips  = 5;
    static const uint32_t kBRDFLUTSize    = 512;
};

}