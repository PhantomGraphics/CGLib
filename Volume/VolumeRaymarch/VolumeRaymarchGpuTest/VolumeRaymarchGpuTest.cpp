// GPU raymarcher / sun-transmittance compute vs the CPU reference (VolumeScattering).
// Needs a real Vulkan device; see VulkanTestFixture.

#include "VulkanTestFixture.h"

#include "../VolumeRaymarchGpu.h"
#include "../../../VulkanGraphics/VulkanSPVResolver.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>

using namespace Phantom::Volume;
using namespace Phantom::VKG;
namespace VS = Phantom::Volume::VolumeScattering;

namespace
{
constexpr VkFormat kTargetFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr uint32_t kW = 32, kH = 32;

float halfToFloat(uint16_t h)
{
  const uint32_t sign = (h >> 15) & 1, exp = (h >> 10) & 0x1f, man = h & 0x3ff;
  float v;
  if (exp == 0) v = std::ldexp(static_cast<float>(man), -24);
  else if (exp == 31) v = man ? NAN : INFINITY;
  else v = std::ldexp(static_cast<float>(man | 0x400), static_cast<int>(exp) - 25);
  return sign ? -v : v;
}

ScalarGrid3D makeBlob(uint32_t n, float peak)
{
  ScalarGridDesc d;
  d.nx = d.ny = d.nz = n;
  d.cellSize = 1.0f;
  ScalarGrid3D g(d);
  const float c = 0.5f * static_cast<float>(n), r = 0.4f * static_cast<float>(n);
  for (uint32_t k = 0; k < n; ++k)
    for (uint32_t j = 0; j < n; ++j)
      for (uint32_t i = 0; i < n; ++i) {
        const glm::vec3 p = g.cellCenter(i, j, k) - glm::vec3(c);
        const float q = glm::length(p) / r;
        g.at(i, j, k) = q < 1.0f ? peak * (1.0f - q * q) : 0.0f;
      }
  return g;
}

// Small RGBA16F render target with a TRANSFER_SRC final layout, for reading the raymarch back.
struct Target {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;

  bool create(const VulkanContext& ctx)
  {
    VkDevice device = ctx.getDevice();
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = kTargetFormat;
    ci.extent = { kW, kH, 1 };
    ci.mipLevels = ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &ci, nullptr, &image) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image, &req);
    auto type = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!type) return false;
    VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, *type };
    if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindImageMemory(device, image, memory, 0);
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = kTargetFormat;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &vi, nullptr, &view) != VK_SUCCESS) return false;

    VkAttachmentDescription att{};
    att.format = kTargetFormat;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &att;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    if (vkCreateRenderPass(device, &rp, nullptr, &renderPass) != VK_SUCCESS) return false;
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = renderPass;
    fb.attachmentCount = 1;
    fb.pAttachments = &view;
    fb.width = kW;
    fb.height = kH;
    fb.layers = 1;
    return vkCreateFramebuffer(device, &fb, nullptr, &framebuffer) == VK_SUCCESS;
  }

  void destroy(const VulkanContext& ctx)
  {
    VkDevice device = ctx.getDevice();
    if (framebuffer) vkDestroyFramebuffer(device, framebuffer, nullptr);
    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
    if (view) vkDestroyImageView(device, view, nullptr);
    if (image) vkDestroyImage(device, image, nullptr);
    if (memory) vkFreeMemory(device, memory, nullptr);
    *this = Target();
  }
};

class VolumeRaymarchGpuTest : public VulkanTestFixture {
protected:
  void SetUp() override
  {
    VulkanTestFixture::SetUp();
    if (::testing::Test::HasFatalFailure()) return;
    if (!Volume3DImage::isSupported(ctx_)) {
      GTEST_SKIP() << "R32_SFLOAT 3D linear/storage not supported on this device";
    }
    ASSERT_TRUE(target_.create(ctx_));
    VolumeRaymarchGpu::Config cfg;
    cfg.fullscreenVertSpv = loadSPVRepo("shaders/volume_fullscreen.vert.spv");
    cfg.raymarchFragSpv = loadSPVRepo("shaders/volume_raymarch.frag.spv");
    cfg.sunTransmittanceCompSpv = loadSPVRepo("shaders/volume_sun_transmittance.comp.spv");
    ASSERT_FALSE(cfg.raymarchFragSpv.empty()) << "shaders missing next to the test binary";
    cfg.renderPass = target_.renderPass;
    ASSERT_TRUE(gpu_.create(ctx_, pool_, cfg));
    ready_ = true;
  }

  void TearDown() override
  {
    if (ctx_.getDevice()) vkDeviceWaitIdle(ctx_.getDevice());
    if (ready_) {
      gpu_.destroy(ctx_);
      target_.destroy(ctx_);
    }
    VulkanTestFixture::TearDown();
  }

  Target target_;
  VolumeRaymarchGpu gpu_;
  bool ready_ = false;
};
}

TEST_F(VolumeRaymarchGpuTest, SunTransmittanceMatchesCpuReference)
{
  const ScalarGrid3D density = makeBlob(16, 3.0f);
  ScatteringParams p;
  p.extinction = 0.4f;
  p.sunDirection = glm::normalize(glm::vec3(0.3f, 0.2f, 1.0f));
  p.stepLength = 0.25f;

  ASSERT_TRUE(gpu_.setGrid(ctx_, pool_, density.desc()));
  ASSERT_TRUE(gpu_.uploadDensity(ctx_, pool_, density));

  VkCommandBuffer cmd = pool_.beginSingleTimeCommands();
  gpu_.recordSunTransmittance(cmd, p);
  pool_.endSingleTimeCommands(cmd);

  std::vector<float> gpu;
  ASSERT_TRUE(gpu_.sunTransmittance().download(ctx_, pool_, gpu));
  const ScalarGrid3D cpu = VS::computeSunTransmittance(density, p);
  ASSERT_EQ(gpu.size(), cpu.data().size());
  float maxErr = 0.0f;
  float minT = 1.0f;
  for (size_t i = 0; i < gpu.size(); ++i) {
    maxErr = std::max(maxErr, std::abs(gpu[i] - cpu.data()[i]));
    minT = std::min(minT, gpu[i]);
  }
  EXPECT_LT(maxErr, 2.0e-3f);
  EXPECT_LT(minT, 0.6f);   // the test actually exercises shadowing
}

TEST_F(VolumeRaymarchGpuTest, RaymarchMatchesCpuReferencePerPixel)
{
  const ScalarGrid3D density = makeBlob(16, 3.0f);
  ScatteringParams p;
  p.extinction = 0.3f;
  p.albedo = 0.9f;
  p.phaseG = 0.5f;
  p.sunDirection = glm::normalize(glm::vec3(0.4f, 0.3f, 0.8f));
  p.sunIrradiance = 1.5f;
  p.ambient = 0.1f;
  p.stepLength = 0.25f;
  p.minTransmittance = 1.0e-3f;

  ASSERT_TRUE(gpu_.setGrid(ctx_, pool_, density.desc()));
  ASSERT_TRUE(gpu_.uploadDensity(ctx_, pool_, density));

  VolumeRaymarchGpu::Camera cam;
  cam.position = glm::vec3(8.0f, 8.0f, -30.0f);
  const glm::mat4 view = glm::lookAt(cam.position, glm::vec3(8.0f, 8.0f, 8.0f), glm::vec3(0, 1, 0));
  const glm::mat4 proj = glm::perspective(glm::radians(28.0f), 1.0f, 0.1f, 200.0f);
  cam.invViewProj = glm::inverse(proj * view);

  VkCommandBuffer cmd = pool_.beginSingleTimeCommands();
  gpu_.recordSunTransmittance(cmd, p);
  const VkClearValue clear{};
  VkRenderPassBeginInfo rb{};
  rb.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rb.renderPass = target_.renderPass;
  rb.framebuffer = target_.framebuffer;
  rb.renderArea = { { 0, 0 }, { kW, kH } };
  rb.clearValueCount = 1;
  rb.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport vp{ 0, 0, static_cast<float>(kW), static_cast<float>(kH), 0.0f, 1.0f };
  VkRect2D sc{ { 0, 0 }, { kW, kH } };
  vkCmdSetViewport(cmd, 0, 1, &vp);
  vkCmdSetScissor(cmd, 0, 1, &sc);
  gpu_.recordRaymarch(cmd, 0, cam, p);
  vkCmdEndRenderPass(cmd);
  pool_.endSingleTimeCommands(cmd);

  // Read back RGBA16F.
  VkDevice device = ctx_.getDevice();
  const VkDeviceSize bytes = static_cast<VkDeviceSize>(kW) * kH * 8;
  VkBuffer buf = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_SHARING_MODE_EXCLUSIVE, 0, nullptr };
  ASSERT_EQ(vkCreateBuffer(device, &bi, nullptr, &buf), VK_SUCCESS);
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device, buf, &req);
  auto type = ctx_.findMemoryType(req.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  ASSERT_TRUE(type.has_value());
  VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, *type };
  ASSERT_EQ(vkAllocateMemory(device, &mai, nullptr, &mem), VK_SUCCESS);
  vkBindBufferMemory(device, buf, mem, 0);
  cmd = pool_.beginSingleTimeCommands();
  VkBufferImageCopy region{};
  region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
  region.imageExtent = { kW, kH, 1 };
  vkCmdCopyImageToBuffer(cmd, target_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &region);
  pool_.endSingleTimeCommands(cmd);
  void* mapped = nullptr;
  ASSERT_EQ(vkMapMemory(device, mem, 0, bytes, 0, &mapped), VK_SUCCESS);
  std::vector<uint16_t> texels(static_cast<size_t>(kW) * kH * 4);
  std::memcpy(texels.data(), mapped, bytes);
  vkUnmapMemory(device, mem);
  vkDestroyBuffer(device, buf, nullptr);
  vkFreeMemory(device, mem, nullptr);

  const ScalarGrid3D sunT = VS::computeSunTransmittance(density, p);
  float maxErr = 0.0f;
  int covered = 0;
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      const glm::vec2 ndc((x + 0.5f) / kW * 2.0f - 1.0f, (y + 0.5f) / kH * 2.0f - 1.0f);
      glm::vec4 n = cam.invViewProj * glm::vec4(ndc, 0.0f, 1.0f);
      glm::vec4 f = cam.invViewProj * glm::vec4(ndc, 1.0f, 1.0f);
      const glm::vec3 dir = glm::normalize(glm::vec3(f) / f.w - glm::vec3(n) / n.w);
      const RayResult ref = VS::marchRay(density, sunT, p, cam.position, dir);

      const uint16_t* px = &texels[(static_cast<size_t>(y) * kW + x) * 4];
      const float r = halfToFloat(px[0]), a = halfToFloat(px[3]);
      maxErr = std::max({ maxErr, std::abs(r - ref.radiance.x) - 0.01f * ref.radiance.x,
                          std::abs(a - (1.0f - ref.transmittance)) - 0.01f });
      if (ref.transmittance < 0.95f) ++covered;
    }
  }
  EXPECT_GT(covered, 50);          // the blob covers a meaningful part of the image
  EXPECT_LT(maxErr, 6.0e-3f);      // half-float output + trilinear hardware rounding
}
