#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../CGLib/VulkanGraphics/VulkanPipeline.h"

#include <vector>

namespace Phantom::VKG { class VulkanContext; }

namespace Phantom::Particles {

// Renders a ParticleSimulator's SSBO as camera-facing billboard sprites: no vertex/index
// buffer is bound, the vertex shader derives a quad corner from gl_VertexIndex % 6 and the
// particle index from gl_VertexIndex / 6 (see shaders/particle_billboard.vert), reading
// position/velocity(.w=size)/color directly from the same storage buffer ParticleSimulator
// writes. Managed directly by the owning app/renderer (not an IVkSubRenderer), matching
// CGApp/CGStudio/GridRenderer's pattern.
class ParticleBillboardRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    ParticleBillboardRenderer() = default;
    ParticleBillboardRenderer(const ParticleBillboardRenderer&) = delete;
    ParticleBillboardRenderer& operator=(const ParticleBillboardRenderer&) = delete;

    void setShaders(Shaders s) { shaders_ = std::move(s); }

    // particleBuffer/maxParticles come from ParticleSimulator::getParticleBuffer()/getMaxParticles();
    // the simulator must already be created.
    void onInit(const Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                VkBuffer particleBuffer, uint32_t maxParticles);

    void setCamera(const glm::mat4& view, const glm::mat4& proj);
    void onUpdate(uint32_t frameIndex);
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex);
    void onCleanup(VkDevice device);

private:
    struct CameraUBO {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 camRight; // xyz
        glm::vec4 camUp;    // xyz
    };

    Shaders shaders_;

    Phantom::VKG::VulkanDescriptorSetLayout dsl_;
    Phantom::VKG::VulkanDescriptorPool      descPool_;
    std::vector<VkDescriptorSet>            descSets_;
    std::vector<Phantom::VKG::VulkanBuffer> cameraUBO_;
    Phantom::VKG::VulkanPipeline             pipeline_;

    CameraUBO cameraData_{};
    uint32_t  maxParticles_   = 0;
    uint32_t  framesInFlight_ = 0;
};

} // namespace Phantom::Particles
