#include "GltfMesh.h"

#include "../Gltf/GltfAccessorView.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <cstdio>
#include <glm/gtc/matrix_inverse.hpp>

using namespace Phantom::Gltf;

// ============================================================
//  Vertex layout descriptors
// ============================================================

VkVertexInputBindingDescription GltfGpuMesh::Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bd{};
    bd.binding   = 0;
    bd.stride    = sizeof(Vertex);
    bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bd;
}

std::vector<VkVertexInputAttributeDescription> GltfGpuMesh::Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attrs(8);
    // location 0: position
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, position);
    // location 1: normal
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, normal);
    // location 2: texcoord
    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset   = offsetof(Vertex, texCoord);
    // location 3: tangent
    attrs[3].binding  = 0;
    attrs[3].location = 3;
    attrs[3].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[3].offset   = offsetof(Vertex, tangent);
    // location 4: joint indices (GPU skinning)
    attrs[4].binding  = 0;
    attrs[4].location = 4;
    attrs[4].format   = VK_FORMAT_R32G32B32A32_SINT;
    attrs[4].offset   = offsetof(Vertex, jointIndices);
    // location 5: joint weights (GPU skinning)
    attrs[5].binding  = 0;
    attrs[5].location = 5;
    attrs[5].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[5].offset   = offsetof(Vertex, jointWeights);
    // location 6: texcoord 1 (secondary UV set)
    attrs[6].binding  = 0;
    attrs[6].location = 6;
    attrs[6].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[6].offset   = offsetof(Vertex, texCoord1);
    // location 7: vertex color (COLOR_0)
    attrs[7].binding  = 0;
    attrs[7].location = 7;
    attrs[7].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[7].offset   = offsetof(Vertex, color);
    return attrs;
}

// ============================================================
//  Build GPU buffers from GltfPrimitive
// ============================================================

bool GltfGpuMesh::build(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                         const GltfDocument& doc, const GltfPrimitive& prim,
                         const glm::mat4& worldTransform)
{
    if (prim.positionAccessor < 0) {
        std::fprintf(stderr, "[GltfMesh] primitive has no POSITION\n");
        return false;
    }

    GltfAccessorView posView(doc, prim.positionAccessor);
    vertexCount_ = static_cast<uint32_t>(posView.count());

    bool hasNormal  = (prim.normalAccessor >= 0);
    bool hasTex     = (prim.texCoord0Accessor >= 0);
    bool hasTex1    = (prim.texCoord1Accessor >= 0);
    bool hasTangent = (prim.tangentAccessor >= 0);
    bool hasJoints  = (prim.jointsAccessor >= 0 && prim.weightsAccessor >= 0);
    bool hasColor   = (prim.colorAccessor >= 0);

    GltfAccessorView normView   (doc, hasNormal  ? prim.normalAccessor    : prim.positionAccessor);
    GltfAccessorView texView    (doc, hasTex     ? prim.texCoord0Accessor : prim.positionAccessor);
    GltfAccessorView tex1View   (doc, hasTex1    ? prim.texCoord1Accessor : prim.positionAccessor);
    GltfAccessorView tanView    (doc, hasTangent ? prim.tangentAccessor   : prim.positionAccessor);
    GltfAccessorView jointsView (doc, hasJoints  ? prim.jointsAccessor    : prim.positionAccessor);
    GltfAccessorView weightsView(doc, hasJoints  ? prim.weightsAccessor   : prim.positionAccessor);
    GltfAccessorView colorView  (doc, hasColor   ? prim.colorAccessor     : prim.positionAccessor);

    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

    std::vector<Vertex> vertices(vertexCount_);
    for (uint32_t i = 0; i < vertexCount_; ++i) {
        const glm::vec3 pos = posView.get<glm::vec3>(i);
        vertices[i].position = glm::vec3(worldTransform * glm::vec4(pos, 1.0f));

        glm::vec3 n = hasNormal ? normView.get<glm::vec3>(i) : glm::vec3(0, 1, 0);
        vertices[i].normal = glm::normalize(normalMatrix * n);

        vertices[i].texCoord  = hasTex  ? texView.get<glm::vec2>(i)  : glm::vec2(0, 0);
        vertices[i].texCoord1 = hasTex1 ? tex1View.get<glm::vec2>(i) : vertices[i].texCoord;
        vertices[i].color     = hasColor ? colorView.get<glm::vec4>(i) : glm::vec4(1.f, 1.f, 1.f, 1.f);

        if (hasTangent) {
            glm::vec4 t = tanView.get<glm::vec4>(i);
            glm::vec3 t3 = glm::normalize(normalMatrix * glm::vec3(t));
            vertices[i].tangent = glm::vec4(t3, t.w);
        } else {
            vertices[i].tangent = glm::vec4(1, 0, 0, 1);
        }

        if (hasJoints) {
            vertices[i].jointIndices = jointsView.get<glm::ivec4>(i);
            vertices[i].jointWeights = weightsView.get<glm::vec4>(i);
        } else {
            // Unskinned primitive: bind to joint 0 with full weight. GltfSceneRenderer keeps
            // bone 0 as identity when no skin matrices have been supplied, so this is a no-op
            // (see GltfSceneRenderer::onUpdate()).
            vertices[i].jointIndices = glm::ivec4(0, 0, 0, 0);
            vertices[i].jointWeights = glm::vec4(1.f, 0.f, 0.f, 0.f);
        }
    }

    vertexBuffer_.create(ctx, pool,
        vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vertices.data());

    bakeTransform_ = worldTransform;
    // Only primitives that will be updated at runtime -- morph targets, or an explicit
    // setKeepCpuVertices(true) for a deforming mesh -- pay for keeping a CPU mirror of the
    // vertex array around after upload.
    if (!prim.targets.empty() || keepCpuVertices_)
        vertices_ = std::move(vertices);

    if (prim.indicesAccessor >= 0) {
        GltfAccessorView idxView(doc, prim.indicesAccessor);
        indexCount_ = static_cast<uint32_t>(idxView.count());

        const auto& acc = doc.accessors[prim.indicesAccessor];
        if (acc.componentType == GltfComponentType::UnsignedShort) {
            indexType_ = VK_INDEX_TYPE_UINT16;
            std::vector<uint16_t> indices(indexCount_);
            for (uint32_t i = 0; i < indexCount_; ++i)
                indices[i] = idxView.get<uint16_t>(i);
            indexBuffer_.create(ctx, pool,
                indices.size() * sizeof(uint16_t),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                indices.data());
        } else {
            indexType_ = VK_INDEX_TYPE_UINT32;
            std::vector<uint32_t> indices(indexCount_);
            for (uint32_t i = 0; i < indexCount_; ++i) {
                if (acc.componentType == GltfComponentType::UnsignedByte)
                    indices[i] = idxView.get<uint8_t>(i);
                else
                    indices[i] = idxView.get<uint32_t>(i);
            }
            indexBuffer_.create(ctx, pool,
                indices.size() * sizeof(uint32_t),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                indices.data());
        }
    }
    return true;
}

bool GltfGpuMesh::updatePositions(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                   const std::vector<glm::vec3>& positions)
{
    if (vertices_.empty() || positions.size() != vertices_.size()) {
        std::fprintf(stderr, "[GltfMesh] updatePositions: size mismatch (got %zu, expected %zu)\n",
                     positions.size(), vertices_.size());
        return false;
    }

    for (size_t i = 0; i < vertices_.size(); ++i)
        vertices_[i].position = glm::vec3(bakeTransform_ * glm::vec4(positions[i], 1.f));

    return vertexBuffer_.upload(ctx, pool, vertices_.data(), vertices_.size() * sizeof(Vertex));
}

bool GltfGpuMesh::updatePositionsAndNormals(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                            const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals)
{
    if (vertices_.empty() || positions.size() != vertices_.size() || normals.size() != vertices_.size()) {
        std::fprintf(stderr, "[GltfMesh] updatePositionsAndNormals: size mismatch (got %zu/%zu, expected %zu)\n",
                     positions.size(), normals.size(), vertices_.size());
        return false;
    }

    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(bakeTransform_)));
    for (size_t i = 0; i < vertices_.size(); ++i) {
        vertices_[i].position = glm::vec3(bakeTransform_ * glm::vec4(positions[i], 1.f));
        const glm::vec3 n = normalMatrix * normals[i];
        const float len = glm::length(n);
        vertices_[i].normal = (len > 1e-8f) ? (n / len) : glm::vec3(0.f, 1.f, 0.f);
    }

    return vertexBuffer_.upload(ctx, pool, vertices_.data(), vertices_.size() * sizeof(Vertex));
}

void GltfGpuMesh::destroy(VkDevice device) {
    vertexBuffer_.destroy(device);
    indexBuffer_.destroy(device);
}
