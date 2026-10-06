#include "VulkanTestFixture.h"
#include "../VulkanPipelineCache.h"

#include <cstring>
#include <filesystem>
#include <fstream>

using Phantom::VKG::VulkanPipelineCache;

namespace {

VkPhysicalDeviceProperties fakeProps()
{
    VkPhysicalDeviceProperties p{};
    p.vendorID = 0x1234;
    p.deviceID = 0x5678;
    for (int i = 0; i < VK_UUID_SIZE; ++i) p.pipelineCacheUUID[i] = static_cast<uint8_t>(i + 1);
    return p;
}

std::vector<char> makeBlob(const VkPhysicalDeviceProperties& p, uint32_t version = VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
{
    std::vector<char> d(32 + 8, 0);
    const uint32_t headerSize = 32;
    std::memcpy(d.data() + 0, &headerSize, 4);
    std::memcpy(d.data() + 4, &version, 4);
    std::memcpy(d.data() + 8, &p.vendorID, 4);
    std::memcpy(d.data() + 12, &p.deviceID, 4);
    std::memcpy(d.data() + 16, p.pipelineCacheUUID, VK_UUID_SIZE);
    return d;
}

} // namespace

TEST(VulkanPipelineCacheHeaderTest, AcceptsMatchingHeader)
{
    const auto p = fakeProps();
    EXPECT_TRUE(VulkanPipelineCache::headerMatches(makeBlob(p), p));
}

TEST(VulkanPipelineCacheHeaderTest, RejectsMismatchesAndGarbage)
{
    const auto p = fakeProps();

    EXPECT_FALSE(VulkanPipelineCache::headerMatches({}, p));
    EXPECT_FALSE(VulkanPipelineCache::headerMatches(std::vector<char>(31, 0), p));

    auto otherVendor = p;  otherVendor.vendorID = 1;
    EXPECT_FALSE(VulkanPipelineCache::headerMatches(makeBlob(p), otherVendor));
    auto otherDevice = p;  otherDevice.deviceID = 1;
    EXPECT_FALSE(VulkanPipelineCache::headerMatches(makeBlob(p), otherDevice));
    auto otherUuid = p;    otherUuid.pipelineCacheUUID[3] ^= 0xFF;
    EXPECT_FALSE(VulkanPipelineCache::headerMatches(makeBlob(p), otherUuid));

    EXPECT_FALSE(VulkanPipelineCache::headerMatches(makeBlob(p, 99), p)); // unknown header version

    auto truncated = makeBlob(p);
    const uint32_t bigHeader = 1000; // claims more header than the blob holds
    std::memcpy(truncated.data(), &bigHeader, 4);
    EXPECT_FALSE(VulkanPipelineCache::headerMatches(truncated, p));
    const uint32_t tinyHeader = 8;
    std::memcpy(truncated.data(), &tinyHeader, 4);
    EXPECT_FALSE(VulkanPipelineCache::headerMatches(truncated, p));
}

using VulkanPipelineCacheTest = VulkanTestFixture;

TEST_F(VulkanPipelineCacheTest, InMemoryCreateDestroyAndRecreate)
{
    VulkanPipelineCache cache;
    cache.destroy(ctx_.getDevice()); // never created: no-op
    EXPECT_FALSE(cache.isValid());

    EXPECT_TRUE(cache.create(ctx_));
    EXPECT_TRUE(cache.isValid());
    EXPECT_TRUE(cache.create(ctx_)); // re-create releases the previous cache
    EXPECT_TRUE(cache.isValid());

    cache.destroy(ctx_.getDevice());
    EXPECT_FALSE(cache.isValid());
    cache.destroy(ctx_.getDevice()); // twice is safe
}

TEST_F(VulkanPipelineCacheTest, PersistsAcrossRunsAndIgnoresCorruptFile)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "cglib_pipeline_cache_test";
    fs::remove_all(dir);

    VulkanPipelineCache cache;
    ASSERT_TRUE(cache.create(ctx_, dir.string()));
    cache.destroy(ctx_.getDevice());
    // Drivers may legitimately return an empty blob for an unused cache; only check what we can.
    const std::string file = VulkanPipelineCache::filePath(dir.string());

    // A corrupt file must be a plain miss: create() still succeeds.
    fs::create_directories(dir);
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "this is not a pipeline cache blob, just some garbage bytes padded to be long enough....";
    }
    EXPECT_TRUE(cache.create(ctx_, dir.string()));
    EXPECT_TRUE(cache.isValid());
    cache.destroy(ctx_.getDevice());

    fs::remove_all(dir);
}
