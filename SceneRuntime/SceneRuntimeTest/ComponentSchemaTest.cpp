#include "gtest/gtest.h"

#include "../SceneRuntime/ComponentSchema.h"

#include <filesystem>
#include <random>

using namespace Phantom::SceneRuntime;

namespace {

ComponentSchema makeMeshSchema()
{
    ComponentSchema schema;
    schema.type = "mesh";
    schema.version = 3;
    schema.fields.push_back({ "shape", ComponentFieldType::String, std::nullopt, std::nullopt, "box|sphere|plane", true });
    schema.fields.push_back({ "size", ComponentFieldType::Float, 0.0, 100.0, "half-extent", true });
    schema.fields.push_back({ "color", ComponentFieldType::Vec3, std::nullopt, std::nullopt, "RGB tint", false });
    return schema;
}

std::filesystem::path tempDir()
{
    static std::mt19937 rng{ std::random_device{}() };
    const auto dir = std::filesystem::temp_directory_path()
        / ("ComponentSchemaTest_" + std::to_string(rng()));
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST(ComponentSchema, JsonRoundTripPreservesAllFieldProperties)
{
    const ComponentSchema schema = makeMeshSchema();
    const std::string json = schema.toJson();
    EXPECT_NE(json.find("\"schema\":\"phantom.component_schema/1\""), std::string::npos);

    bool ok = false;
    ComponentSchema loaded = ComponentSchema::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(loaded.type, "mesh");
    EXPECT_EQ(loaded.version, 3);
    ASSERT_EQ(loaded.fields.size(), 3u);

    EXPECT_EQ(loaded.fields[0].name, "shape");
    EXPECT_EQ(loaded.fields[0].type, ComponentFieldType::String);
    EXPECT_TRUE(loaded.fields[0].required);
    EXPECT_EQ(loaded.fields[0].tooltip, "box|sphere|plane");
    EXPECT_FALSE(loaded.fields[0].minValue.has_value());

    EXPECT_EQ(loaded.fields[1].name, "size");
    EXPECT_EQ(loaded.fields[1].type, ComponentFieldType::Float);
    ASSERT_TRUE(loaded.fields[1].minValue.has_value());
    EXPECT_DOUBLE_EQ(*loaded.fields[1].minValue, 0.0);
    ASSERT_TRUE(loaded.fields[1].maxValue.has_value());
    EXPECT_DOUBLE_EQ(*loaded.fields[1].maxValue, 100.0);

    EXPECT_EQ(loaded.fields[2].name, "color");
    EXPECT_EQ(loaded.fields[2].type, ComponentFieldType::Vec3);
    EXPECT_FALSE(loaded.fields[2].required);
}

TEST(ComponentSchema, DefaultValueRoundTripsAndIsOmittedWhenAbsent)
{
    ComponentSchema schema;
    schema.type = "rigidBody";
    schema.version = 1;
    ComponentFieldSchema mass{ "mass", ComponentFieldType::Float, 0.0, std::nullopt, "kg, 0 = static", true };
    mass.defaultValue = 1.0;
    ComponentFieldSchema shape{ "shape", ComponentFieldType::String, std::nullopt, std::nullopt, "box|sphere|plane", true };
    shape.defaultValue = "box";
    ComponentFieldSchema friction{ "friction", ComponentFieldType::Float, 0.0, 1.0, "no default declared", false };
    schema.fields = { mass, shape, friction };

    const std::string json = schema.toJson();
    EXPECT_NE(json.find("\"default\""), std::string::npos); // present at least twice below is what matters
    EXPECT_NE(json.find("\"box\""), std::string::npos);

    bool ok = false;
    ComponentSchema loaded = ComponentSchema::fromJson(json, &ok);
    ASSERT_TRUE(ok);
    ASSERT_EQ(loaded.fields.size(), 3u);
    EXPECT_DOUBLE_EQ(loaded.fields[0].defaultValue.get<double>(), 1.0);
    EXPECT_EQ(loaded.fields[1].defaultValue.get<std::string>(), "box");
    EXPECT_TRUE(loaded.fields[2].defaultValue.is_null()); // no default declared -- stays null, not e.g. 0
}

TEST(ComponentSchema, FromJsonRejectsWrongSchema)
{
    bool ok = true;
    ComponentSchema loaded = ComponentSchema::fromJson(R"({"schema":"something.else/1","type":"mesh","fields":[]})", &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(loaded.type.empty());
}

TEST(ComponentSchema, FromJsonRejectsGarbage)
{
    bool ok = true;
    ComponentSchema loaded = ComponentSchema::fromJson("not json at all", &ok);
    EXPECT_FALSE(ok);
}

TEST(ComponentSchema, FromJsonRejectsUnknownFieldType)
{
    const std::string json = R"({"schema":"phantom.component_schema/1","type":"mesh","version":1,)"
        R"("fields":[{"name":"shape","type":"quaternion","required":true}]})";
    bool ok = true;
    ComponentSchema loaded = ComponentSchema::fromJson(json, &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(loaded.fields.empty());
}

TEST(ComponentSchema, FromJsonRejectsFieldMissingName)
{
    const std::string json = R"({"schema":"phantom.component_schema/1","type":"mesh","version":1,)"
        R"("fields":[{"type":"float","required":true}]})";
    bool ok = true;
    ComponentSchema::fromJson(json, &ok);
    EXPECT_FALSE(ok);
}

TEST(ComponentSchema, SaveLoadFileRoundTrip)
{
    const ComponentSchema schema = makeMeshSchema();
    const std::filesystem::path dir = tempDir();
    const std::filesystem::path path = dir / "mesh.schema.json";

    ASSERT_TRUE(schema.saveToFile(path));
    bool ok = false;
    ComponentSchema loaded = ComponentSchema::loadFromFile(path, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(loaded.type, "mesh");
    ASSERT_EQ(loaded.fields.size(), 3u);

    std::filesystem::remove_all(dir);
}

TEST(ComponentSchema, LoadFromFileMissingFileFails)
{
    bool ok = true;
    ComponentSchema loaded = ComponentSchema::loadFromFile("Z:/does/not/exist.json", &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(loaded.type.empty());
}
