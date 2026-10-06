#include <gtest/gtest.h>

#include "FrameSync.h"

using VKG::FrameSync;

namespace {
VkFence fakeFence(uintptr_t v) { return reinterpret_cast<VkFence>(v); }
}

TEST(FrameSyncTest, ClaimImageReturnsPreviousUser) {
    FrameSync s;
    s.resetImageTable(3);
    EXPECT_EQ(s.claimImage(1, fakeFence(0x10)), VK_NULL_HANDLE);
    EXPECT_EQ(s.claimImage(1, fakeFence(0x20)), fakeFence(0x10));
    EXPECT_EQ(s.claimImage(1, fakeFence(0x20)), fakeFence(0x20));
    EXPECT_EQ(s.claimImage(0, fakeFence(0x30)), VK_NULL_HANDLE);
}

TEST(FrameSyncTest, ClaimImageOutOfRangeIsIgnored) {
    FrameSync s;
    EXPECT_EQ(s.claimImage(0, fakeFence(0x10)), VK_NULL_HANDLE);  // empty table
    s.resetImageTable(2);
    EXPECT_EQ(s.claimImage(2, fakeFence(0x10)), VK_NULL_HANDLE);
    EXPECT_EQ(s.claimImage(99, fakeFence(0x10)), VK_NULL_HANDLE);
}

TEST(FrameSyncTest, ResetImageTableForgetsFences) {
    FrameSync s;
    s.resetImageTable(2);
    s.claimImage(0, fakeFence(0x10));
    s.resetImageTable(2);
    EXPECT_EQ(s.claimImage(0, fakeFence(0x20)), VK_NULL_HANDLE);
}

TEST(FrameSyncTest, CreateWithoutDeviceFailsAndLeavesEmpty) {
    FrameSync s;
    EXPECT_FALSE(s.create(VK_NULL_HANDLE, 2, 3));
    EXPECT_EQ(s.frameCount(), 0u);
    EXPECT_EQ(s.imageCount(), 0u);
    s.destroy(VK_NULL_HANDLE);  // no-op
}
