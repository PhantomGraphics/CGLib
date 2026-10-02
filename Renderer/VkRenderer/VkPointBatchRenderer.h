#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#include <cstdint>
#include <map>
#include <vector>

namespace Phantom::VKG {

class VulkanContext;
class VulkanCommandPool;

/// @brief CPU-side points of one batch. Pointers are only read during addBatch().
struct PointBatchData {
    const float* positions = nullptr; ///< xyz interleaved, @c count points.
    const float* colors    = nullptr; ///< rgb interleaved (linear), or null => defaultColor.
    const float* radii     = nullptr; ///< one world-space radius per point, or null => defaultRadius.
    uint64_t     count     = 0;
    float        defaultColor[3] = { 0.8f, 0.8f, 0.8f };
    float        defaultRadius   = 0.01f;
};

/// @brief One draw of a batch.
struct PointBatchDraw {
    uint32_t  batch = 0;
    glm::mat4 model{ 1.f };
    bool      useTint = false;               ///< true: draw every point with @c tint instead of its own colour.
    glm::vec3 tint{ 1.f, 1.f, 1.f };
    float     radiusScale = 1.f;
};

/// @brief Draws many independent point sets ("batches") as round, world-radius point sprites.
///
/// Shared by Universe / PhantomStudio / FluidStudio for VDB Points: each batch owns an interleaved
/// device-local vertex buffer (pos, colour, radius), and every draw supplies its own model matrix,
/// optional uniform colour and radius scale through push constants. Radius 0 is not drawn.
/// Depth-tested, opaque. Viewport/scissor must be set by the caller (same as VkPointRenderer).
///
/// Batches are immutable: replace data by removeBatch() + addBatch(). Both call
/// vkDeviceWaitIdle() (rare, load-time operations); render() never allocates.
/// Colours are written as-is (linear) unless setSrgbOutput(true).
class VkPointBatchRenderer {
public:
    struct Config {
        std::vector<uint32_t> vertSpv; ///< point_batch.vert.spv
        std::vector<uint32_t> fragSpv; ///< point_batch.frag.spv
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    explicit VkPointBatchRenderer(Config config) : config_(std::move(config)) {}

    bool create(const VulkanContext& ctx, const VulkanCommandPool& pool, VkRenderPass renderPass);
    void destroy(VkDevice device);
    bool isValid() const { return pipeline_.getPipeline() != VK_NULL_HANDLE; }

    /// @return batch id (> 0), or 0 on failure (no data, or buffer creation failed).
    uint32_t addBatch(const PointBatchData& data);
    void     removeBatch(uint32_t id);
    uint64_t batchPointCount(uint32_t id) const;
    uint64_t gpuBytes() const;

    /// @brief Per-frame camera. @p viewportHeightPx is the render target height in pixels.
    void setCamera(const glm::mat4& view, const glm::mat4& proj, uint32_t viewportHeightPx);
    void setSrgbOutput(bool v) { srgbOutput_ = v; }
    void setMinPointSize(float px) { minPointSize_ = px; }

    /// @brief Record draws. Unknown batch ids are skipped.
    void render(VkCommandBuffer cmd, const std::vector<PointBatchDraw>& draws) const;

private:
    struct Batch { VulkanBuffer buffer; uint32_t count = 0; uint64_t bytes = 0; };

    Config   config_;
    const VulkanContext*     ctx_  = nullptr;
    const VulkanCommandPool* pool_ = nullptr;
    VulkanPipeline pipeline_;
    std::map<uint32_t, Batch> batches_;
    uint32_t nextId_ = 1;

    glm::mat4 viewProj_{ 1.f };
    float     pixelsPerUnit_ = 400.f;
    float     minPointSize_  = 1.f;
    bool      srgbOutput_    = false;
};

} // namespace Phantom::VKG
