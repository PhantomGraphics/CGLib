#include <gtest/gtest.h>

#include "CGLib/Editor/Input/ActionState.h"
#include "CGLib/Editor/Input/InputResolver.h"

using namespace Phantom::Editor;

namespace {
InputSpec input(const std::string& text) {
    InputSpec spec;
    std::string error;
    EXPECT_TRUE(parseInput(text, spec, error)) << error;
    return spec;
}

void bind(BindingTable& table, const std::string& context, const std::string& line) {
    std::string ctx = context, error;
    ASSERT_TRUE(applyKeymapLine(table, ctx, line, error)) << error;
}
}

TEST(ActionStateTest, ArbitraryNamesAndOverridesSurviveFrameUpdates) {
    ActionState state;
    EXPECT_EQ(state.value("custom_action"), 0.f);
    state.setValue("custom_action", 0.75f);
    EXPECT_TRUE(state.isPressed("custom_action"));
    state.inject("custom_action", false);
    state.setValue("custom_action", 1.f);
    EXPECT_EQ(state.value("custom_action"), 0.f);
    state.inject("another_action", true);
    EXPECT_EQ(state.value("another_action"), 1.f);
    state.clearInjected();
    EXPECT_EQ(state.value("custom_action"), 1.f);
    EXPECT_EQ(state.value("another_action"), 0.f);
    state.setValue("custom_action", 0.5f);
    EXPECT_FALSE(state.isPressed("custom_action"));
}

TEST(InputResolverTest, ContextModifiersAndReleaseArePreserved) {
    BindingTable table;
    bind(table, "global", "bind ctrl+key.S \"global save\"");
    bind(table, "view", "bind ctrl+key.S \"view save\"");
    bind(table, "view", "bind key.F \"fire\" state=release");
    EXPECT_EQ(triggerCommands(table, "view", input("ctrl+key.S"), false),
              (std::vector<std::string>{"view save"}));
    EXPECT_EQ(triggerCommands(table, "play", input("ctrl+key.S"), false),
              (std::vector<std::string>{"global save"}));
    EXPECT_TRUE(triggerCommands(table, "view", input("key.S"), false).empty());
    EXPECT_TRUE(triggerCommands(table, "view", input("key.F"), false).empty());
    EXPECT_EQ(triggerCommands(table, "view", input("key.F"), true),
              (std::vector<std::string>{"fire"}));
    const auto commands = triggerCommands(table, "view", input("ctrl+key.S"), false);
    table.clear();
    EXPECT_EQ(commands.front(), "view save");
}

TEST(InputResolverTest, DeltaSubstitutionAndActionBindingsStaySeparate) {
    BindingTable table;
    bind(table, "view", "bind mouse.left.drag \"orbit dx=$dx dy=$dy\" scale=0.5");
    bind(table, "view", "bind mouse.wheel \"zoom by=$value\" scale=2");
    bind(table, "view", "bind key.W action=custom_forward");
    EXPECT_EQ(deltaCommands(table, "view", input("mouse.left.drag"), 8, -4, 0),
              (std::vector<std::string>{"orbit dx=4 dy=-2"}));
    EXPECT_EQ(deltaCommands(table, "view", input("mouse.wheel"), 0, 0, -3),
              (std::vector<std::string>{"zoom by=-6"}));
    EXPECT_TRUE(triggerCommands(table, "view", input("key.W"), false).empty());
}

TEST(InputResolverTest, ActionsRetainGlobalCombinationInversionScalingAndClamp) {
    BindingTable table;
    bind(table, "global", "bind key.W action=forward scale=0.25");
    bind(table, "play", "bind key.W action=forward scale=0.5");
    bind(table, "play", "bind pad.lstick.y action=forward invert=1 scale=2");
    bind(table, "play", "bind pad.lstick.y action=backward");
    bind(table, "view", "bind key.W action=editor_action");
    bind(table, "play", "bind key.F \"fire\"");
    auto values = resolveActions(table, "play", [](const InputSpec& spec) {
        return spec.kind == InputKind::Axis ? -0.75f : 1.f;
    });
    EXPECT_EQ(values.size(), 2u);
    EXPECT_EQ(values["forward"], 1.f);
    EXPECT_EQ(values["backward"], 0.f);
    values = resolveActions(table, "play", [](const InputSpec& spec) {
        return spec.kind == InputKind::Axis ? 0.4f : 1.f;
    });
    EXPECT_EQ(values["forward"], 0.5f);
    EXPECT_EQ(values["backward"], 0.4f);
    EXPECT_TRUE(resolveActions(table, "play", {}).empty());
}
