#include "VectorFieldRenderer.h"

#include "../Volume/SparseVolumeTree/Interpolator.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include "../../../CGLib/Graphics/ColorMap.h"
#include "../../../CGLib/Graphics/ColorTable.h"

#include "../../../CGLib/Math/Box3d.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <unordered_set>

namespace VkVolumeView {

namespace {

struct GridPoint {
    int x;
    int y;
    int z;

    bool operator==(const GridPoint& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    bool operator<(const GridPoint& rhs) const {
        if (x != rhs.x) return x < rhs.x;
        if (y != rhs.y) return y < rhs.y;
        return z < rhs.z;
    }
};

struct GridEdge {
    GridPoint a;
    GridPoint b;

    bool operator==(const GridEdge& rhs) const {
        return a == rhs.a && b == rhs.b;
    }
};

struct GridEdgeHash {
    size_t operator()(const GridEdge& e) const {
        auto h = [](const GridPoint& p) -> size_t {
            const uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(p.x));
            const uint64_t y = static_cast<uint64_t>(static_cast<uint32_t>(p.y));
            const uint64_t z = static_cast<uint64_t>(static_cast<uint32_t>(p.z));
            return static_cast<size_t>((x * 73856093ull) ^ (y * 19349663ull) ^ (z * 83492791ull));
        };
        return h(e.a) ^ (h(e.b) << 1);
    }
};

GridEdge normalizeEdge(const GridPoint& p0, const GridPoint& p1) {
    if (p1 < p0) {
        return GridEdge{ p1, p0 };
    }
    return GridEdge{ p0, p1 };
}

glm::vec3 toWorldPos(const GridPoint& p, const float voxelSize) {
    return glm::vec3(
        static_cast<float>(p.x) * voxelSize,
        static_cast<float>(p.y) * voxelSize,
        static_cast<float>(p.z) * voxelSize);
}

void addGridBoxLines(std::vector<LineVertex>& vertices,
                     const glm::vec3& minPos,
                     const glm::vec3& maxPos,
                     const glm::vec4& color) {
    const glm::vec3 p000(minPos.x, minPos.y, minPos.z);
    const glm::vec3 p100(maxPos.x, minPos.y, minPos.z);
    const glm::vec3 p010(minPos.x, maxPos.y, minPos.z);
    const glm::vec3 p110(maxPos.x, maxPos.y, minPos.z);
    const glm::vec3 p001(minPos.x, minPos.y, maxPos.z);
    const glm::vec3 p101(maxPos.x, minPos.y, maxPos.z);
    const glm::vec3 p011(minPos.x, maxPos.y, maxPos.z);
    const glm::vec3 p111(maxPos.x, maxPos.y, maxPos.z);

    auto addLine = [&](const glm::vec3& a, const glm::vec3& b) {
        vertices.push_back(LineVertex{a, color});
        vertices.push_back(LineVertex{b, color});
    };

    addLine(p000, p100);
    addLine(p010, p110);
    addLine(p001, p101);
    addLine(p011, p111);

    addLine(p000, p010);
    addLine(p100, p110);
    addLine(p001, p011);
    addLine(p101, p111);

    addLine(p000, p001);
    addLine(p100, p101);
    addLine(p010, p011);
    addLine(p110, p111);
}

void addVoxelCellLines(std::vector<LineVertex>& vertices,
                       std::unordered_set<GridEdge, GridEdgeHash>& uniqueEdges,
                       const Phantom::Volume::Coord& index,
                       const float voxelSize,
                       const glm::vec4& color) {
    const GridPoint p000{ index.x, index.y, index.z };
    const GridPoint p100{ index.x + 1, index.y, index.z };
    const GridPoint p010{ index.x, index.y + 1, index.z };
    const GridPoint p110{ index.x + 1, index.y + 1, index.z };
    const GridPoint p001{ index.x, index.y, index.z + 1 };
    const GridPoint p101{ index.x + 1, index.y, index.z + 1 };
    const GridPoint p011{ index.x, index.y + 1, index.z + 1 };
    const GridPoint p111{ index.x + 1, index.y + 1, index.z + 1 };

    const std::array<std::pair<GridPoint, GridPoint>, 12> edges = {
        std::make_pair(p000, p100), std::make_pair(p010, p110), std::make_pair(p001, p101), std::make_pair(p011, p111),
        std::make_pair(p000, p010), std::make_pair(p100, p110), std::make_pair(p001, p011), std::make_pair(p101, p111),
        std::make_pair(p000, p001), std::make_pair(p100, p101), std::make_pair(p010, p011), std::make_pair(p110, p111)
    };

    for (const auto& edge : edges) {
        const GridEdge e = normalizeEdge(edge.first, edge.second);
        if (!uniqueEdges.insert(e).second) {
            continue;
        }

        vertices.push_back(LineVertex{ toWorldPos(e.a, voxelSize), color });
        vertices.push_back(LineVertex{ toWorldPos(e.b, voxelSize), color });
    }
}

} // namespace

void VectorFieldRenderer::setShowVectorField(const bool show) {
    if (showVectorField_ == show) {
        return;
    }
    showVectorField_ = show;
    dirty_ = true;
}

void VectorFieldRenderer::setShowVolumeGrid(const bool show) {
    if (showVolumeGrid_ == show) {
        return;
    }
    showVolumeGrid_ = show;
    dirty_ = true;
}

void VectorFieldRenderer::onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                   VkRenderPass renderPass, uint32_t framesInFlight) {
    ctx_ = &ctx;
    pool_ = &pool;

    pipeline_.create(ctx, renderPass, framesInFlight,
                     std::move(shaders_.vertSpv), std::move(shaders_.fragSpv));
    dirty_ = true;
}

void VectorFieldRenderer::onUpdate(uint32_t frameIndex) {
    if (!ctx_ || !pool_) {
        return;
    }

    if (dirty_) {
        rebuildLines();
        dirty_ = false;
    }

    LinePipeline::UBO ubo{};
    ubo.mvp = computeMVP();
    pipeline_.updateUBO(frameIndex, ubo);
}

void VectorFieldRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!enabled_ || vertices_.empty() || !vertexBuffer_.isValid()) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer vbuf = vertexBuffer_.getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

    const VkDescriptorSet set = pipeline_.getDescriptorSet(frameIndex);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, &set, 0, nullptr);

    vkCmdDraw(cmd, static_cast<uint32_t>(vertices_.size()), 1, 0, 0);
}

void VectorFieldRenderer::onCleanup(VkDevice device) {
    vertexBuffer_.destroy(device);
    pipeline_.destroy(device);
    vertices_.clear();
}

glm::mat4 VectorFieldRenderer::computeMVP() const {
    const float az = glm::radians(camera_.azimuth);
    const float el = glm::radians(camera_.elevation);

    const glm::vec3 target(0.0f, 0.0f, 0.0f);
    const glm::vec3 eye(
        camera_.distance * std::cos(el) * std::sin(az),
        camera_.distance * std::sin(el),
        camera_.distance * std::cos(el) * std::cos(az));

    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));

    const float aspect = (extent_.height > 0)
        ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
        : 1.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 1000.0f);
    proj[1][1] *= -1.0f;

    return proj * view;
}

void VectorFieldRenderer::rebuildLines() {
    vertices_.clear();

    if (!world_ || !ctx_ || !pool_) {
        return;
    }

    Phantom::Graphics::ColorMap colorMap(0.0f, 1.0f, Phantom::Graphics::ColorTable::createJetTable(256));
    const glm::vec4 gridColor(0.85f, 0.85f, 0.85f, 1.0f);

    const auto& scenes = world_->getScenes();
    for (const auto& scene : scenes) {
        if (!scene || !scene->getShape() || !scene->isVisible()) {
            continue;
        }

        if (showVectorField_) {
            Phantom::Volume::TrilinearInterpolator<float> interp(*scene->getShape());
            scene->getShape()->forEachActive([&](const Phantom::Volume::Coord&, const Phantom::Math::Vector3df& worldPos, float) {
                const auto g = interp.getGradient(worldPos);

                const glm::vec3 start(worldPos.x, worldPos.y, worldPos.z);
                const glm::vec3 grad(g.x, g.y, g.z);
                const glm::vec3 end = start + grad * scale_;

                const float mag = glm::length(grad);
                const auto c = colorMap.getInterpolatedColor(mag);
                const glm::vec4 color(c.x, c.y, c.z, 1.0f);

                vertices_.push_back(LineVertex{start, color});
                vertices_.push_back(LineVertex{end, color});
            });
        }

        if (showVolumeGrid_ && scene->getShape()->getActiveVoxelCount() > 0) {
            std::unordered_set<GridEdge, GridEdgeHash> uniqueEdges;
            const float voxelSize = scene->getShape()->getVoxelSize();

            scene->getShape()->forEachActive([&](const Phantom::Volume::Coord& index, const Phantom::Math::Vector3df&, float) {
                addVoxelCellLines(vertices_, uniqueEdges, index, voxelSize, gridColor);
            });
        }
    }

    vertexBuffer_.destroy(ctx_->getDevice());
    if (vertices_.empty()) {
        return;
    }

    vertexBuffer_.create(*ctx_, *pool_,
                         sizeof(LineVertex) * vertices_.size(),
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vertices_.data());
}

} // namespace VkVolumeView
