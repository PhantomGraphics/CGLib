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

TEST(AssetId, GenerateIsValid)
{
    AssetId id = AssetId::generate();
    EXPECT_TRUE(id.isValid());
}

TEST(AssetId, GenerateProducesUuidV4Shape)
{
    const std::string v = AssetId::generate().value();
    ASSERT_EQ(v.size(), 36u);
    EXPECT_EQ(v[8], '-');
    EXPECT_EQ(v[13], '-');
    EXPECT_EQ(v[14], '4'); // RFC 4122 version 4
    EXPECT_EQ(v[18], '-');
    EXPECT_TRUE(v[19] == '8' || v[19] == '9' || v[19] == 'a' || v[19] == 'b'); // RFC 4122 variant 10xx
    EXPECT_EQ(v[23], '-');
    for (char c : v) {
        EXPECT_TRUE(c == '-' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(AssetId, GenerateProducesDistinctIds)
{
    std::unordered_set<AssetId> ids;
    for (int i = 0; i < 1000; ++i) ids.insert(AssetId::generate());
    EXPECT_EQ(ids.size(), 1000u);
}
