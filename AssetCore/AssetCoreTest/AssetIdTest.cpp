#include "gtest/gtest.h"

#include "../AssetCore/AssetId.h"

#include <unordered_set>

using Phantom::Asset::AssetId;

TEST(AssetId, DefaultIsInvalid)
{
    AssetId id;
    EXPECT_FALSE(id.isValid());
    EXPECT_TRUE(id.value().empty());
}

TEST(AssetId, ValueRoundTrips)
{
    AssetId id("8f4c1d2e-0000-0000-0000-000000000000");
    EXPECT_TRUE(id.isValid());
    EXPECT_EQ(id.value(), "8f4c1d2e-0000-0000-0000-000000000000");
}

TEST(AssetId, EqualityIsExactStringComparison)
{
    EXPECT_EQ(AssetId("a"), AssetId("a"));
    EXPECT_NE(AssetId("a"), AssetId("b"));
    EXPECT_NE(AssetId("a"), AssetId());
}

TEST(AssetId, UsableInHashSet)
{
    std::unordered_set<AssetId> ids;
    ids.insert(AssetId("a"));
    ids.insert(AssetId("a"));
    ids.insert(AssetId("b"));
    EXPECT_EQ(ids.size(), 2u);
}

TEST(AssetId, UsableInOrderedContainer)
{
    EXPECT_TRUE(AssetId("a") < AssetId("b"));
    EXPECT_FALSE(AssetId("b") < AssetId("a"));
}
