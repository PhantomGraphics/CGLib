#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../Gltf/GltfDocument.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"

#include <vector>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Gltf
{

    // Per-primitive GPU geometry (vertex + index buffer)
    class GltfGpuMesh {
    public:
        struct Vertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 texCoord;
            glm::vec4 tangent;
            glm::ivec4 jointIndices{0, 0, 0, 0};  // GPU skinning; (0,0,0,0)+(1,0,0,0) weight = no-op
            glm::vec4  jointWeights{1.f, 0.f, 0.f, 0.f};

            static VkVertexInputBindingDescription getBindingDescription();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };

        // Returns false (no GPU resources created) if the primitive has no POSITION accessor.
        bool build(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
            const GltfDocument& doc, const GltfPrimitive& prim,
            const glm::mat4& worldTransform);
        void destroy(VkDevice device);

        // Rewrites this primitive's position attribute (only) and re-uploads the whole vertex
        // buffer -- see class comment. `positions` must be in the same space/order as the
        // POSITION accessor build() read (i.e. the output of Phantom::Gltf::applyMorphs(),
        // GltfMorphApply.h) and the same size as what build() saw; call only on primitives that
        // have morph targets (GltfPrimitive::targets non-empty). Re-applies the same
        // worldTransform build() baked in, so callers never need to track it themselves.
        // Returns false (no-op) if build() was never called or positions.size() mismatches.
        bool updatePositions(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
            const std::vector<glm::vec3>& positions);

        VkBuffer    vertexBuffer() const { return vertexBuffer_.get(); }
        VkBuffer    indexBuffer()  const { return indexBuffer_.get(); }
        uint32_t    indexCount()   const { return indexCount_; }
        bool        hasIndices()   const { return indexCount_ > 0; }
        uint32_t    vertexCount()  const { return vertexCount_; }
        VkIndexType indexType()    const { return indexType_; }

    private:
        Phantom::VKG::VulkanBuffer vertexBuffer_;
        Phantom::VKG::VulkanBuffer indexBuffer_;
        uint32_t          indexCount_ = 0;
        uint32_t          vertexCount_ = 0;
        VkIndexType       indexType_ = VK_INDEX_TYPE_UINT32;

        // CPU-side mirror of the last-uploaded vertex buffer, kept only so updatePositions() can
        // rewrite the position field alone while re-sending normal/texCoord/tangent/joint data
        // unchanged (build() itself does not need this after the initial upload). Empty unless
        // this primitive has morph targets.
        std::vector<Vertex> vertices_;
        glm::mat4            bakeTransform_ = glm::mat4(1.f); // worldTransform build() applied
    };

}