#include "gtest/gtest.h"

#include "../SceneRuntime/ComponentSchemaRegistry.h"

#include <filesystem>
#include <random>

using namespace Phantom::SceneRuntime;

namespace {

ComponentSchema makeMeshSchema()
{
    ComponentSchema schema;
    schema.type = "mesh";
    schema.version = 1;
    schema.fields.push_back({ "shape", ComponentFieldType::String, std::nullopt, std::nullopt, "box|sphere|plane", true });
    schema.fields.push_back({ "size", ComponentFieldType::Float, 0.0, 100.0, "half-extent", true });
    schema.fields.push_back({ "color", ComponentFieldType::Vec3, std::nullopt, std::nullopt, "RGB tint", false });
    return schema;
}

ComponentSchema makeRigidBodySchema()
{
    ComponentSchema schema;
    schema.type = "rigidBody";
    schema.version = 1;
    schema.fields.push_back({ "mass", ComponentFieldType::Float, 0.0, std::nullopt, "kg", true });
    return schema;
}

std::filesystem::path tempDir()
{
    static std::mt19937 rng{ std::random_device{}() };
    const auto dir = std::filesystem::temp_directory_path()
        / ("ComponentSchemaRegistryTest_" + std::to_string(rng()));
    std::filesystem::create_directories(dir);
    return dir;
}

ComponentRecord makeMeshComponent(const nlohmann::json& data)
{
    ComponentRecord c;
    c.type = "mesh";
    c.data = data;
    return c;
}

bool hasIssueKind(const std::vector<ComponentValidationIssue>& issues, ComponentValidationIssue::Kind kind)
{
    for (const auto& issue : issues) {
        if (issue.kind == kind) return true;
    }
    return false;
}

} // namespace

TEST(ComponentSchemaRegistry, RegisterAndFind)
{
    ComponentSchemaRegistry registry;
    EXPECT_EQ(registry.size(), 0u);
    registry.registerSchema(makeMeshSchema());
    EXPECT_EQ(registry.size(), 1u);
    const ComponentSchema* found = registry.find("mesh");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->version, 1);
    EXPECT_EQ(registry.find("missing"), nullptr);
}

TEST(ComponentSchemaRegistry, RegisterReplacesSameType)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentSchema v2 = makeMeshSchema();
    v2.version = 2;
    registry.registerSchema(v2);
    EXPECT_EQ(registry.size(), 1u);
    EXPECT_EQ(registry.find("mesh")->version, 2);
}

TEST(ComponentSchemaRegistry, ValidateFullyValidComponentHasNoIssues)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentRecord c = makeMeshComponent({ { "shape", "box" }, { "size", 1.5 }, { "color", { 1.0, 0.5, 0.0 } } });
    EXPECT_TRUE(registry.validate(c).empty());
}

TEST(ComponentSchemaRegistry, ValidateOmittedOptionalFieldHasNoIssues)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentRecord c = makeMeshComponent({ { "shape", "sphere" }, { "size", 2.0 } });
    EXPECT_TRUE(registry.validate(c).empty());
}

TEST(ComponentSchemaRegistry, ValidateUnknownComponentType)
{
    ComponentSchemaRegistry registry;
    ComponentRecord c = makeMeshComponent({ { "shape", "box" } });
    auto issues = registry.validate(c);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, ComponentValidationIssue::Kind::UnknownComponentType);
}

TEST(ComponentSchemaRegistry, ValidateMissingRequiredField)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentRecord c = makeMeshComponent({ { "shape", "box" } }); // "size" missing
    auto issues = registry.validate(c);
    EXPECT_TRUE(hasIssueKind(issues, ComponentValidationIssue::Kind::MissingRequiredField));
}

TEST(ComponentSchemaRegistry, ValidateWrongType)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentRecord c = makeMeshComponent({ { "shape", "box" }, { "size", "not-a-number" } });
    auto issues = registry.validate(c);
    EXPECT_TRUE(hasIssueKind(issues, ComponentValidationIssue::Kind::WrongType));
}

TEST(ComponentSchemaRegistry, ValidateOutOfRange)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentRecord c = makeMeshComponent({ { "shape", "box" }, { "size", 999.0 } });
    auto issues = registry.validate(c);
    EXPECT_TRUE(hasIssueKind(issues, ComponentValidationIssue::Kind::OutOfRange));
}

TEST(ComponentSchemaRegistry, ValidateUnknownFieldIsFlaggedNotDropped)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    ComponentRecord c = makeMeshComponent({ { "shape", "box" }, { "size", 1.0 }, { "mysteryField", 42 } });
    auto issues = registry.validate(c);
    ASSERT_TRUE(hasIssueKind(issues, ComponentValidationIssue::Kind::UnknownField));
    // Flagging is non-destructive -- the caller's ComponentRecord itself is never mutated.
    EXPECT_TRUE(c.data.contains("mysteryField"));
}

TEST(ComponentSchemaRegistry, JsonRoundTripPreservesEveryRegisteredSchema)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    registry.registerSchema(makeRigidBodySchema());

    const std::string json = registry.toJson();
    EXPECT_NE(json.find("\"schema\":\"phantom.component_schema_registry/1\""), std::string::npos);

    bool ok = false;
    ComponentSchemaRegistry loaded = ComponentSchemaRegistry::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(loaded.size(), 2u);
    ASSERT_NE(loaded.find("mesh"), nullptr);
    EXPECT_EQ(loaded.find("mesh")->fields.size(), 3u);
    ASSERT_NE(loaded.find("rigidBody"), nullptr);
    EXPECT_EQ(loaded.find("rigidBody")->fields.size(), 1u);
}

TEST(ComponentSchemaRegistry, FromJsonRejectsWrongSchema)
{
    bool ok = true;
    ComponentSchemaRegistry loaded = ComponentSchemaRegistry::fromJson(R"({"schema":"something.else/1","schemas":[]})", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.size(), 0u);
}

TEST(ComponentSchemaRegistry, FromJsonRejectsGarbage)
{
    bool ok = true;
    ComponentSchemaRegistry loaded = ComponentSchemaRegistry::fromJson("not json at all", &ok);
    EXPECT_FALSE(ok);
}

TEST(ComponentSchemaRegistry, FromJsonRejectsWholeFileOnOneBadEmbeddedSchema)
{
    const std::string json = R"({"schema":"phantom.component_schema_registry/1","schemas":[)"
        R"({"schema":"phantom.component_schema/1","type":"mesh","version":1,"fields":[]},)"
        R"({"schema":"phantom.component_schema/1","type":"broken","version":1,)"
        R"("fields":[{"name":"x","type":"quaternion","required":true}]})"
        R"(]})";
    bool ok = true;
    ComponentSchemaRegistry loaded = ComponentSchemaRegistry::fromJson(json, &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.size(), 0u); // the valid "mesh" schema is not partially kept
}

TEST(ComponentSchemaRegistry, SaveLoadFileRoundTrip)
{
    ComponentSchemaRegistry registry;
    registry.registerSchema(makeMeshSchema());
    const std::filesystem::path dir = tempDir();
    const std::filesystem::path path = dir / "registry.json";

    ASSERT_TRUE(registry.saveToFile(path));
    bool ok = false;
    ComponentSchemaRegistry loaded = ComponentSchemaRegistry::loadFromFile(path, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(loaded.size(), 1u);

    std::filesystem::remove_all(dir);
}

TEST(ComponentSchemaRegistry, LoadFromFileMissingFileFails)
{
    bool ok = true;
    ComponentSchemaRegistry loaded = ComponentSchemaRegistry::loadFromFile("Z:/does/not/exist.json", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.size(), 0u);
}
