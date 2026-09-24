#include "gtest/gtest.h"

#include "../AssetCore/AssetManifest.h"
#include "../AssetCore/ContentHash.h"

#include <filesystem>
#include <random>

using namespace Phantom::Asset;

namespace {

AssetManifestEntry makeEntry(const std::string& id, const std::string& uri)
{
    AssetManifestEntry e;
    e.id = AssetId(id);
    e.uri = *AssetUri::parse(uri);
    e.contentHash = ContentHash("sha256:deadbeef");
    e.sourceBlend = "Sources/Blender/room.blend";
    return e;
}

std::filesystem::path tempDir()
{
    static std::mt19937 rng{ std::random_device{}() };
    const auto dir = std::filesystem::temp_directory_path()
        / ("AssetManifestTest_" + std::to_string(rng()));
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST(AssetManifest, UpsertFindRemove)
{
    AssetManifest m;
    EXPECT_EQ(m.size(), 0u);
    m.upsert(makeEntry("id-1", "Assets/a.glb"));
    EXPECT_EQ(m.size(), 1u);
    const AssetManifestEntry* found = m.find(AssetId("id-1"));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->uri.value(), "Assets/a.glb");
    EXPECT_EQ(m.find(AssetId("missing")), nullptr);
    EXPECT_TRUE(m.remove(AssetId("id-1")));
    EXPECT_EQ(m.size(), 0u);
    EXPECT_FALSE(m.remove(AssetId("id-1")));
}

TEST(AssetManifest, UpsertReplacesSameId)
{
    AssetManifest m;
    m.upsert(makeEntry("id-1", "Assets/a.glb"));
    m.upsert(makeEntry("id-1", "Assets/b.glb"));
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.find(AssetId("id-1"))->uri.value(), "Assets/b.glb");
}

TEST(AssetManifest, JsonRoundTrip)
{
    AssetManifest m;
    AssetManifestEntry e = makeEntry("id-1", "Assets/a.glb");
    e.dependencies = { AssetId("id-2"), AssetId("id-3") };
    m.upsert(e);
    m.upsert(makeEntry("id-2", "Assets/b.glb"));

    const std::string json = m.toJson();
    EXPECT_NE(json.find("\"schema\":\"phantom.manifest/1\""), std::string::npos);

    bool ok = false;
    AssetManifest loaded = AssetManifest::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(loaded.size(), 2u);
    const AssetManifestEntry* loadedEntry = loaded.find(AssetId("id-1"));
    ASSERT_NE(loadedEntry, nullptr);
    EXPECT_EQ(loadedEntry->uri.value(), "Assets/a.glb");
    EXPECT_EQ(loadedEntry->contentHash.value(), "sha256:deadbeef");
    EXPECT_EQ(loadedEntry->sourceBlend, "Sources/Blender/room.blend");
    ASSERT_EQ(loadedEntry->dependencies.size(), 2u);
    EXPECT_EQ(loadedEntry->dependencies[0], AssetId("id-2"));
    EXPECT_EQ(loadedEntry->dependencies[1], AssetId("id-3"));
}

TEST(AssetManifest, FromJsonRejectsWrongSchema)
{
    bool ok = true;
    AssetManifest loaded = AssetManifest::fromJson(R"({"schema":"something.else/1","assets":[]})", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.size(), 0u);
}

TEST(AssetManifest, FromJsonRejectsGarbage)
{
    bool ok = true;
    AssetManifest loaded = AssetManifest::fromJson("not json at all", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.size(), 0u);
}

TEST(AssetManifest, SaveLoadFileRoundTrip)
{
    AssetManifest m;
    m.upsert(makeEntry("id-1", "Assets/a.glb"));
    const std::filesystem::path dir = tempDir();
    const std::filesystem::path path = dir / "manifest.json";

    ASSERT_TRUE(m.saveToFile(path));
    bool ok = false;
    AssetManifest loaded = AssetManifest::loadFromFile(path, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded.find(AssetId("id-1"))->uri.value(), "Assets/a.glb");

    std::filesystem::remove_all(dir);
}

TEST(AssetManifest, LoadFromFileMissingFileFails)
{
    bool ok = true;
    AssetManifest loaded = AssetManifest::loadFromFile("Z:/does/not/exist.json", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.size(), 0u);
}

TEST(ContentHash, KnownSha256Vectors)
{
    const auto empty = ContentHash::fromBytes("", 0);
    EXPECT_EQ(empty.value(), "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    const std::string abc = "abc";
    const auto three = ContentHash::fromBytes(abc.data(), abc.size());
    EXPECT_EQ(three.value(), "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
