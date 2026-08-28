#include "DenseVolumeRenderer.h"

#include "../../../CGLib/Graphics/ColorMap.h"
#include "../../../CGLib/Graphics/ColorTable.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <limits>

namespace VkVolumeView {

namespace {

Phantom::Graphics::ColorTable createGrayscaleTable(const int resolution) {
	Phantom::Graphics::ColorTable table(resolution);
	for (int i = 0; i < resolution; ++i) {
		const float t = (resolution > 1)
			? static_cast<float>(i) / static_cast<float>(resolution - 1)
			: 0.0f;
		table.setColor(i, Phantom::Graphics::ColorRGBAf(t, t, t, 1.0f));
	}
	return table;
}

Phantom::Graphics::ColorTable createViridisTable(const int resolution) {
	Phantom::Graphics::ColorTable table(resolution);
	for (int i = 0; i < resolution; ++i) {
		const float t = (resolution > 1)
			? static_cast<float>(i) / static_cast<float>(resolution - 1)
			: 0.0f;

		const float r = 0.267f + t * (0.993f - 0.267f);
		const float g = 0.005f + t * (0.906f - 0.005f);
		const float b = 0.329f + t * (0.144f - 0.329f);
		table.setColor(i, Phantom::Graphics::ColorRGBAf(r, g, b, 1.0f));
	}
	return table;
}

Phantom::Graphics::ColorMap createColorMap(
	DenseVolumeRenderer::ColorMapType type,
	const float minV,
	const float maxV)
{
	constexpr int kTableRes = 256;
	switch (type) {
	case DenseVolumeRenderer::ColorMapType::Viridis:
		return Phantom::Graphics::ColorMap(minV, maxV, createViridisTable(kTableRes));
	case DenseVolumeRenderer::ColorMapType::Grayscale:
		return Phantom::Graphics::ColorMap(minV, maxV, createGrayscaleTable(kTableRes));
	case DenseVolumeRenderer::ColorMapType::Jet:
	default:
		return Phantom::Graphics::ColorMap(minV, maxV, Phantom::Graphics::ColorTable::createJetTable(kTableRes));
	}
}

} // anonymous namespace

void DenseVolumeRenderer::onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
								   VkRenderPass renderPass, uint32_t framesInFlight) {
	ctx_ = &ctx;
	pool_ = &pool;
	framesInFlight_ = framesInFlight;

	pipeline_.create(ctx, renderPass, framesInFlight,
					 std::move(shaders_.vertSpv), std::move(shaders_.fragSpv));
	dirty_ = true;
}

void DenseVolumeRenderer::onUpdate(uint32_t frameIndex) {
	if (!ctx_ || !pool_) return;

	if (dirty_) {
		rebuildVertices();
		dirty_ = false;
	}

	VolumePipeline::UBO ubo{};
	ubo.mvp = computeMVP();
	ubo.pointSize = pointSize_;
	pipeline_.updateUBO(frameIndex, ubo);
}

void DenseVolumeRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
	if (!enabled_ || vertices_.empty() || !vertexBuffer_.isValid()) return;

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

	VkBuffer vbuf = vertexBuffer_.getBuffer();
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

	const VkDescriptorSet set = pipeline_.getDescriptorSet(frameIndex);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipeline_.getLayout(), 0, 1, &set, 0, nullptr);

	vkCmdDraw(cmd, static_cast<uint32_t>(vertices_.size()), 1, 0, 0);
}

void DenseVolumeRenderer::onCleanup(VkDevice device) {
	vertexBuffer_.destroy(device);
	pipeline_.destroy(device);
	vertices_.clear();
}

void DenseVolumeRenderer::onImGui() {
}

glm::mat4 DenseVolumeRenderer::computeMVP() const {
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

void DenseVolumeRenderer::rebuildVertices() {
	vertices_.clear();

	if (!world_ || !ctx_ || !pool_) return;

	float minV = std::numeric_limits<float>::max();
	float maxV = std::numeric_limits<float>::lowest();

	for (const auto& scene : world_->getDenseScenes()) {
		if (!scene || !scene->isVisible() || !scene->getVolume()) continue;

		const auto res = scene->getVolume()->getResolutions();
		for (size_t i = 0; i < res[0]; ++i) {
			for (size_t j = 0; j < res[1]; ++j) {
				for (size_t k = 0; k < res[2]; ++k) {
					const float v = scene->getVolume()->getValue({(int)i, (int)j, (int)k});
					minV = std::min(minV, v);
					maxV = std::max(maxV, v);
				}
			}
		}
	}

	if (minV > maxV) {
		minV = 0.0f;
		maxV = 1.0f;
	}
	if (minV == maxV) {
		maxV = minV + 1.0f;
	}

	auto colorMap = createColorMap(colorMapType_, minV, maxV);

	for (const auto& scene : world_->getDenseScenes()) {
		if (!scene || !scene->isVisible() || !scene->getVolume()) continue;

		const auto* vol = scene->getVolume();
		const auto res = vol->getResolutions();

		for (size_t i = 0; i < res[0]; ++i) {
			for (size_t j = 0; j < res[1]; ++j) {
				for (size_t k = 0; k < res[2]; ++k) {
					const float v = vol->getValue({(int)i, (int)j, (int)k});
					const auto p = vol->getCellPosition(i, j, k);
					const auto c = colorMap.getInterpolatedColor(v);

					PointVertex vert{};
					vert.pos = glm::vec3(p.x, p.y, p.z);
					vert.color = glm::vec4(c.x, c.y, c.z, 1.0f);
					vertices_.push_back(vert);
				}
			}
		}
	}

	vertexBuffer_.destroy(ctx_->getDevice());
	if (vertices_.empty()) return;

	vertexBuffer_.create(*ctx_, *pool_,
						 sizeof(PointVertex) * vertices_.size(),
						 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
						 vertices_.data());
}

} // namespace VkVolumeView
