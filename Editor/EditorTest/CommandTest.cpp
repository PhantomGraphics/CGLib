#include <gtest/gtest.h>

#include "../Input/Bindings.h"
#include "../Input/Input.h"
#include "../Commands/CommandRegistry.h"
#include "../Commands/Response.h"
#include "../Commands/Script.h"
#include "../Commands/References.h"
#include "../Selection/SelectionSet.h"
#include "../Commands/Tokenizer.h"

using namespace Phantom::Editor::CommandCore;
using namespace Phantom::Editor;
using IntSelectionSet = Phantom::Editor::SelectionSet<int>;

namespace {
InputSpec input(const std::string& text) {
    InputSpec spec;
    std::string error;
    EXPECT_TRUE(parseInput(text, spec, error)) << text << ": " << error;
    return spec;
}

const CommandRegistry& testRegistry() {
    static const CommandRegistry registry({
        {"paint", "colour,pt", "appearance", "Paint objects", true, true,
            {{"color", ArgumentType::Color, "", 0.0, 1.0}, {"amount", ArgumentType::Number, "0.5", 0.0, 1.0},
             {"count", ArgumentType::Integer, "1", 1, 10}, {"at", ArgumentType::Vector3, "0,0,0", kNoMin, kNoMax},
             {"on", ArgumentType::Boolean, "", kNoMin, kNoMax}}},
        {"list", "", "query", "List things", false, false, {}},
    });
    return registry;
}
}

// --- Selection -----------------------------------------------------------------------------

TEST(SelectionSetTest, ReplaceAddToggleKeepOrderWithoutDuplicates) {
    IntSelectionSet sel;
    sel.replace({5, 2, 5});
    EXPECT_EQ(sel.ids(), (std::vector<int>{5, 2}));
    sel.add({2, 7});
    EXPECT_EQ(sel.ids(), (std::vector<int>{5, 2, 7}));
    sel.toggle({2, 9});
    EXPECT_EQ(sel.ids(), (std::vector<int>{5, 7, 9}));
    sel.prune([](int id) { return id != 7; });
    EXPECT_EQ(sel.ids(), (std::vector<int>{5, 9}));
}

TEST(SelectionSetTest, GlobAndIdReferences) {
    EXPECT_TRUE(globMatch("Box.*", "Box.001"));
    EXPECT_TRUE(globMatch("?ox", "Box"));
    EXPECT_FALSE(globMatch("Box", "Box.001"));
    int id = -1;
    EXPECT_TRUE(parseIdReference("@12", id));
    EXPECT_EQ(id, 12);
    EXPECT_FALSE(parseIdReference("@", id));
    EXPECT_FALSE(parseIdReference("@-1", id));
    EXPECT_FALSE(parseIdReference("@1x", id));
    EXPECT_FALSE(parseIdReference("12", id));
}

// --- Response ------------------------------------------------------------------------------

TEST(ResponseTest, SuccessAndFailureShape) {
    const Json ok = success({{"n", 1}});
    EXPECT_TRUE(good(ok));
    EXPECT_EQ(ok["result"]["n"], 1);
    const Json bad = failure(ErrorCode::NotFound, "nope");
    EXPECT_FALSE(good(bad));
    EXPECT_EQ(bad["error"]["code"], "not_found");
    EXPECT_EQ(bad["error"]["message"], "nope");
}

// --- Script --------------------------------------------------------------------------------

TEST(ScriptTest, SplitsLinesKeepingBlanksAndStrippingBomAndCr) {
    const auto lines = splitScriptLines("\xEF\xBB\xBF" "add box\r\n\r\n# c\nundo\n");
    EXPECT_EQ(lines, (std::vector<std::string>{"add box", "", "# c", "undo"}));
    EXPECT_TRUE(splitScriptLines("").empty());
    EXPECT_EQ(splitScriptLines("a"), (std::vector<std::string>{"a"}));
    std::vector<std::string> out;
    EXPECT_FALSE(readScriptLines("Z:\\definitely\\missing.phs", out));
}

// --- Registry ------------------------------------------------------------------------------

TEST(RegistryTest, FindsByNameAndAliasCaseInsensitively) {
    const auto& r = testRegistry();
    ASSERT_NE(r.find("PAINT"), nullptr);
    EXPECT_EQ(r.find("colour")->name, std::string("paint"));
    EXPECT_EQ(r.find("pt")->name, std::string("paint"));
    EXPECT_EQ(r.find("nope"), nullptr);
}

TEST(RegistryTest, ValidatesOptionsFromTheSchema) {
    const auto& paint = *testRegistry().find("paint");
    std::string error;
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "amount", "0.25", error));
    EXPECT_FALSE(CommandRegistry::validateOption(paint, "amount", "2", error));
    EXPECT_FALSE(CommandRegistry::validateOption(paint, "count", "1.5", error));
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "count", "3", error));
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "color", "#ff8800", error));
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "color", "1,0.5,0", error));
    EXPECT_FALSE(CommandRegistry::validateOption(paint, "color", "2,0,0", error));
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "at", "1,2,3", error));
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "at", "2", error));
    EXPECT_FALSE(CommandRegistry::validateOption(paint, "at", "1,2", error));
    EXPECT_TRUE(CommandRegistry::validateOption(paint, "on", "TRUE", error));
    EXPECT_FALSE(CommandRegistry::validateOption(paint, "on", "yes", error));
    EXPECT_FALSE(CommandRegistry::validateOption(paint, "bogus", "1", error));
    EXPECT_EQ(error, "unknown option bogus");
}

TEST(RegistryTest, DescribeHelpAndReferenceComeFromOneSchema) {
    const auto& r = testRegistry();
    const Json d = Json::parse(CommandRegistry::describe(*r.find("paint")));
    EXPECT_EQ(d["name"], "paint");
    EXPECT_EQ(d["aliases"].size(), 2u);
    EXPECT_EQ(d["arguments"][1]["default"], "0.5");
    EXPECT_EQ(d["arguments"][1]["min"], 0.0);
    EXPECT_FALSE(d["arguments"][3].contains("min"));
    EXPECT_EQ(Json::parse(r.describeAll()).size(), 2u);
    EXPECT_NE(r.help(nullptr).find("list - List things"), std::string::npos);
    EXPECT_NE(r.help(r.find("paint")).find("amount=number"), std::string::npos);
    const std::string md = r.referenceMarkdown("Test reference");
    EXPECT_EQ(md.rfind("# Test reference", 0), 0u);
    EXPECT_NE(md.find("## `paint`"), std::string::npos);
}

