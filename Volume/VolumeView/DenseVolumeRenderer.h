#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "VolumePipeline.h"
#include "SparseVolumeRenderer.h"
#include "World.h"

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace VkVolumeView {

class DenseVolumeRenderer : public ::VKG::IVkSubRenderer {
public:
	enum class ColorMapType {
		Jet = 0,
		Viridis = 1,
		Grayscale = 2,
	};

	struct Shaders {
		std::vector<uint32_t> vertSpv;
		std::vector<uint32_t> fragSpv;
	};

	void setWorld(World* world) { world_ = world; }
	void setExtent(VkExtent2D ext) { extent_ = ext; }
	void syncCamera(const SparseVolumeRenderer::CameraState& cam) { camera_ = cam; }
	void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }
	void setEnabled(bool e) { enabled_ = e; }
	bool isEnabled() const { return enabled_; }

	float getPointSize() const { return pointSize_; }
	void  setPointSize(float s) { pointSize_ = std::clamp(s, 1.0f, 20.0f); }

	ColorMapType getColorMapType() const { return colorMapType_; }
	void         setColorMapType(ColorMapType t) { colorMapType_ = t; markDirty(); }

	void markDirty() { dirty_ = true; }

	void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
				VkRenderPass renderPass, uint32_t framesInFlight) override;
	void onUpdate(uint32_t frameIndex) override;
	void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
	void onCleanup(VkDevice device) override;
	void onImGui() override;

private:
	glm::mat4 computeMVP() const;
	void rebuildVertices();

	World* world_ = nullptr;
	const Phantom::VKG::VulkanContext* ctx_ = nullptr;
	const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;
	VkExtent2D extent_{1280, 720};

	uint32_t framesInFlight_ = 2;
	bool dirty_ = true;
	bool enabled_ = false;

	float pointSize_ = 4.0f;
	ColorMapType colorMapType_ = ColorMapType::Jet;
	SparseVolumeRenderer::CameraState camera_;

	Shaders shaders_;
	VolumePipeline pipeline_;
	Phantom::VKG::VulkanBuffer vertexBuffer_;
	std::vector<PointVertex> vertices_;
};

} // namespace VkVolumeView
