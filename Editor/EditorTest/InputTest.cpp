#include <gtest/gtest.h>
#include "CGLib/Editor/Input/Bindings.h"
#include "CGLib/Editor/Commands/Tokenizer.h"
#include "CGLib/Editor/Commands/Script.h"

using namespace Phantom::Editor;
namespace {
InputSpec input(const std::string& text) {
    InputSpec spec;
    std::string error;
    EXPECT_TRUE(parseInput(text, spec, error)) << text << ": " << error;
    return spec;
}

}

// --- Tokenizer -----------------------------------------------------------------------------

TEST(TokenizerTest, QuotedPathsAndColorHashAreOneToken) {
    ParsedCommand parsed;
    std::string error;
    ASSERT_TRUE(tokenize("material @3 color=#ff8800 # comment", parsed, error));
    EXPECT_EQ(parsed.tokens, (std::vector<std::string>{"material", "@3", "color=#ff8800"}));
    ASSERT_TRUE(tokenize("open \"C:\\my scenes\\a.cgstudio\"", parsed, error));
    EXPECT_EQ(parsed.tokens[1], "C:\\my scenes\\a.cgstudio");
    EXPECT_TRUE(parsed.quotedAtStart[1]);
    EXPECT_FALSE(parsed.quotedAtStart[0]);
    EXPECT_FALSE(tokenize("add box name=\"unfinished", parsed, error));
    EXPECT_EQ(error, "unterminated quote");
}

TEST(TokenizerTest, BlankAndCommentLinesYieldNoTokens) {
    ParsedCommand parsed;
    std::string error;
    ASSERT_TRUE(tokenize("   ", parsed, error));
    EXPECT_TRUE(parsed.tokens.empty());
    ASSERT_TRUE(tokenize("# just a comment", parsed, error));
    EXPECT_TRUE(parsed.tokens.empty());
}

TEST(TokenizerTest, QuoteTokenRoundTrips) {
    for (const std::string text : {"camera orbit dx=$dx dy=$dy", "say \"hi\"", "C:\\a\\\\b", ""}) {
        ParsedCommand parsed;
        std::string error;
        ASSERT_TRUE(tokenize("x " + quoteToken(text), parsed, error)) << text;
        ASSERT_EQ(parsed.tokens.size(), 2u) << text;
        EXPECT_EQ(parsed.tokens[1], text);
    }
}

// --- Input notation ------------------------------------------------------------------------

TEST(InputTest, ParsesKeysModifiersMouseAndPad) {
    const InputSpec s = input("ctrl+shift+key.S");
    EXPECT_EQ(s.modifiers, ModCtrl | ModShift);
    EXPECT_EQ(s.device, InputDevice::Key);
    EXPECT_EQ(s.name, "s");
    EXPECT_EQ(input("mouse.left").kind, InputKind::Press);
    EXPECT_EQ(input("mouse.left.click").kind, InputKind::Click);
    EXPECT_EQ(input("alt+mouse.left.drag").kind, InputKind::Drag);
    EXPECT_EQ(input("mouse.move").kind, InputKind::Move);
    EXPECT_EQ(input("mouse.wheel").kind, InputKind::Wheel);
    EXPECT_EQ(input("pad.a").kind, InputKind::Press);
    EXPECT_EQ(input("pad.lstick.x").kind, InputKind::Axis);
    EXPECT_EQ(input("pad.rtrigger").kind, InputKind::Axis);
}

TEST(InputTest, KeyNamesCompareCaseInsensitivelyAndModifiersMatter) {
    EXPECT_TRUE(input("key.W") == input("KEY.w"));
    EXPECT_FALSE(input("key.W") == input("shift+key.W"));
    EXPECT_TRUE(input("shift+ctrl+key.W") == input("ctrl+shift+key.W"));
    EXPECT_FALSE(input("mouse.left") == input("mouse.left.drag"));
}

TEST(InputTest, RejectsMalformedInput) {
    InputSpec spec;
    std::string error;
    for (const char* bad : {"", "W", "key.", "hyper+key.W", "ctrl+ctrl+key.W", "mouse.back", "mouse.left.hold",
                            "pad.lstick", "joy.a", "key.a-b"}) {
        EXPECT_FALSE(parseInput(bad, spec, error)) << bad;
        EXPECT_FALSE(error.empty()) << bad;
    }
}

TEST(InputTest, FormatRoundTrips) {
    for (const char* text : {"key.W", "ctrl+key.S", "ctrl+shift+alt+key.F5", "mouse.left.drag", "shift+mouse.left.click",
                             "mouse.wheel", "mouse.move", "pad.lstick.y", "pad.a"}) {
        EXPECT_EQ(formatInput(input(text)), text);
    }
}

// --- Binding table -------------------------------------------------------------------------

TEST(BindingTableTest, ActiveContextWinsThenGlobal) {
    BindingTable t;
    std::string context, error;
    ASSERT_TRUE(applyKeymapLine(t, context, "context global", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.R \"global reset\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.C \"control toggle\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "context view", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.R \"camera reset\"", error));

    auto r = t.resolve("view", input("key.R"));
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0]->command, "camera reset");
    r = t.resolve("play", input("key.R"));
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0]->command, "global reset");
    r = t.resolve("view", input("key.C"));
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0]->command, "control toggle");
    EXPECT_TRUE(t.resolve("view", input("key.Q")).empty());
    EXPECT_TRUE(t.resolve("", input("key.C")).size() == 1u);
}

TEST(BindingTableTest, ModifiersMustMatchExactly) {
    BindingTable t;
    std::string context, error;
    ASSERT_TRUE(applyKeymapLine(t, context, "context view", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind mouse.left.click \"select pick mode=replace\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind shift+mouse.left.click \"select pick mode=add\"", error));
    EXPECT_EQ(t.resolve("view", input("mouse.left.click"))[0]->command, "select pick mode=replace");
    EXPECT_EQ(t.resolve("view", input("shift+mouse.left.click"))[0]->command, "select pick mode=add");
    EXPECT_TRUE(t.resolve("view", input("ctrl+mouse.left.click")).empty());
}

TEST(BindingTableTest, BindingTypesAreDerivedFromTheInput) {
    BindingTable t;
    std::string context, error;
    ASSERT_TRUE(applyKeymapLine(t, context, "context view", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.R \"camera reset\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind mouse.left.drag \"camera orbit dx=$dx dy=$dy\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.W action=move_forward", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.F \"fire\" state=release", error));
    EXPECT_EQ(t.resolve("view", input("key.R"))[0]->type, BindingType::Trigger);
    EXPECT_EQ(t.resolve("view", input("mouse.left.drag"))[0]->type, BindingType::Delta);
    const Binding* w = t.resolve("view", input("key.W"))[0];
    EXPECT_EQ(w->type, BindingType::Action);
    EXPECT_EQ(w->action, "move_forward");
    EXPECT_TRUE(t.resolve("view", input("key.F"))[0]->onRelease);
}

TEST(BindingTableTest, OneAxisCanDriveSeveralActionsAndRebindReplaces) {
    BindingTable t;
    std::string context, error;
    ASSERT_TRUE(applyKeymapLine(t, context, "context play", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind pad.lstick.y action=move_forward invert=1", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind pad.lstick.y action=move_back", error));
    auto r = t.resolve("play", input("pad.lstick.y"));
    ASSERT_EQ(r.size(), 2u);
    EXPECT_TRUE(r[0]->invert);
    EXPECT_FALSE(r[1]->invert);

    // Re-binding the same action replaces it; a command bind on the same input replaces nothing here.
    ASSERT_TRUE(applyKeymapLine(t, context, "bind pad.lstick.y action=move_back scale=0.5", error));
    r = t.resolve("play", input("pad.lstick.y"));
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[1]->scale, 0.5);

    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.P \"sim pause\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.P \"sim run\"", error));
    r = t.resolve("play", input("key.P"));
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0]->command, "sim run");
}

TEST(BindingTableTest, UnbindAndFind) {
    BindingTable t;
    std::string context, error;
    ASSERT_TRUE(applyKeymapLine(t, context, "context view", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.R \"camera reset\"", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "context play", error));
    ASSERT_TRUE(applyKeymapLine(t, context, "bind key.R \"character reset\"", error));
    EXPECT_EQ(t.find(input("key.R")).size(), 2u);
    ASSERT_TRUE(applyKeymapLine(t, context, "unbind key.R", error));
    EXPECT_EQ(t.find(input("key.R")).size(), 1u);
    EXPECT_EQ(t.find(input("key.R"))[0]->context, "view");
    EXPECT_EQ(t.unbind("view", input("key.R")), 1u);
    EXPECT_TRUE(t.all().empty());
}

TEST(BindingTableTest, ExpandsDeltaPlaceholdersWithScale) {
    Binding b;
    b.command = "camera orbit dx=$dx dy=$dy";
    EXPECT_EQ(expandCommand(b, 4.0, -2.5, 0.0), "camera orbit dx=4 dy=-2.5");
    b.scale = 0.5;
    EXPECT_EQ(expandCommand(b, 4.0, -2.5, 0.0), "camera orbit dx=2 dy=-1.25");
    b.command = "camera zoom by=$value";
    EXPECT_EQ(expandCommand(b, 0, 0, 3.0), "camera zoom by=1.5");
}

TEST(KeymapTest, RejectsBadLinesWithLineNumbers) {
    BindingTable t;
    std::string error;
    EXPECT_FALSE(loadKeymap(t, {"bind key.R \"x\""}, error));
    EXPECT_NE(error.find("line 1"), std::string::npos); // no context yet
    t.clear();
    const std::vector<std::vector<std::string>> bad = {
        {"context view", "bind key.R"},                              // missing command
        {"context view", "bind key.R \"a\" action=jump"},            // command and action
        {"context view", "bind mouse.wheel \"z\" state=release"},    // state on delta
        {"context view", "bind key.R \"a\" scale=2"},                // scale on trigger
        {"context view", "bind key.R action=x invert=maybe"},        // bad bool
        {"context view", "bind hyper+key.R \"a\""},                  // bad input
        {"context view", "frobnicate"},                              // unknown verb
        {"context view", "bind key.A \"a\"", "bind key.B \"unfinished"}, // unterminated quote
    };
    for (const auto& lines : bad) {
        BindingTable table;
        EXPECT_FALSE(loadKeymap(table, lines, error)) << lines.back();
        EXPECT_NE(error.find("line "), std::string::npos) << lines.back();
    }
}

TEST(KeymapTest, BlankAndCommentLinesAreIgnored) {
    BindingTable t;
    std::string error;
    ASSERT_TRUE(loadKeymap(t, splitScriptLines("# default keymap\r\n\r\ncontext view # trailing\r\nbind key.R \"camera reset\"\r\n"), error)) << error;
    EXPECT_EQ(t.all().size(), 1u);
}

TEST(KeymapTest, SaveAndLoadRoundTrips) {
    const char* text =
        "context global\n"
        "bind ctrl+key.Z \"undo\"\n"
        "bind key.C \"control toggle\"\n"
        "\n"
        "context view\n"
        "bind mouse.left.drag \"camera orbit dx=$dx dy=$dy\" scale=0.5\n"
        "bind mouse.wheel \"camera zoom by=$value\"\n"
        "bind key.F \"fire\" state=release\n"
        "\n"
        "context play\n"
        "bind key.W action=move_forward\n"
        "bind pad.lstick.y action=move_forward invert=1\n"
        "bind pad.lstick.y action=move_back scale=2\n";
    BindingTable a;
    std::string error;
    ASSERT_TRUE(loadKeymap(a, splitScriptLines(text), error)) << error;
    EXPECT_EQ(a.all().size(), 8u);
    const std::string saved = saveKeymap(a);
    EXPECT_EQ(saved, text);
    BindingTable b;
    ASSERT_TRUE(loadKeymap(b, splitScriptLines(saved), error)) << error;
    EXPECT_EQ(saveKeymap(b), saved);
}
