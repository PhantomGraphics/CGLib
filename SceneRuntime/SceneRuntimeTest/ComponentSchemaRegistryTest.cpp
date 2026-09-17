#include "gtest/gtest.h"

#include "../SceneRuntime/ComponentSchemaRegistry.h"

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
