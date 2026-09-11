#include "gtest/gtest.h"

#include "../AssetCore/AssetUri.h"

using Phantom::Asset::AssetUri;

TEST(AssetUri, AcceptsAndNormalizesRelativePath)
{
    auto uri = AssetUri::parse("Assets/Generated/room.glb");
    ASSERT_TRUE(uri.has_value());
    EXPECT_EQ(uri->value(), "Assets/Generated/room.glb");
}

TEST(AssetUri, NormalizesBackslashesToForwardSlashes)
{
    auto uri = AssetUri::parse("Assets\\Generated\\room.glb");
    ASSERT_TRUE(uri.has_value());
    EXPECT_EQ(uri->value(), "Assets/Generated/room.glb");
}

TEST(AssetUri, RejectsEmpty)
{
    EXPECT_FALSE(AssetUri::parse("").has_value());
}

TEST(AssetUri, RejectsAbsolutePosixPath)
{
    EXPECT_FALSE(AssetUri::parse("/etc/passwd").has_value());
}

TEST(AssetUri, RejectsWindowsDriveLetter)
{
    EXPECT_FALSE(AssetUri::parse("C:/Users/name/model.glb").has_value());
    EXPECT_FALSE(AssetUri::parse("C:\\Users\\name\\model.glb").has_value());
}

TEST(AssetUri, RejectsHomeRelative)
{
    EXPECT_FALSE(AssetUri::parse("~/model.glb").has_value());
}

TEST(AssetUri, RejectsParentEscape)
{
    EXPECT_FALSE(AssetUri::parse("../outside/model.glb").has_value());
    EXPECT_FALSE(AssetUri::parse("Assets/../../outside.glb").has_value());
}

TEST(AssetUri, RejectsCurrentDirSegment)
{
    EXPECT_FALSE(AssetUri::parse("./model.glb").has_value());
    EXPECT_FALSE(AssetUri::parse("Assets/./model.glb").has_value());
}

TEST(AssetUri, RejectsDoubleSlashEmptySegment)
{
    EXPECT_FALSE(AssetUri::parse("Assets//model.glb").has_value());
}

TEST(AssetUri, ToPathJoinsAgainstProjectRoot)
{
    auto uri = AssetUri::parse("Assets/Generated/room.glb");
    ASSERT_TRUE(uri.has_value());
    const std::filesystem::path root = std::filesystem::path("C:/Project");
    const std::filesystem::path expected = root / "Assets" / "Generated" / "room.glb";
    EXPECT_EQ(uri->toPath(root), expected);
}

TEST(AssetUri, EqualityIsValueBased)
{
    EXPECT_EQ(*AssetUri::parse("a/b.glb"), *AssetUri::parse("a/b.glb"));
    EXPECT_NE(*AssetUri::parse("a/b.glb"), *AssetUri::parse("a/c.glb"));
}
