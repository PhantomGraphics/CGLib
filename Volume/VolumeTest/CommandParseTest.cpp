#include <gtest/gtest.h>

#include "CommandParse.h"

using namespace VolumeView;

TEST(CommandParseTest, SplitKeepsEmptyFields)
{
    EXPECT_EQ(splitBy("a:b:c", ':'), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(splitBy("a::b", ':'), (std::vector<std::string>{"a", "", "b"}));
    EXPECT_EQ(splitBy("", ':'), (std::vector<std::string>{""}));
    EXPECT_EQ(splitBy("x,y", ','), (std::vector<std::string>{"x", "y"}));
}

TEST(CommandParseTest, ParseNumbers)
{
    float f = 0.f;
    int i = 0;
    EXPECT_TRUE(parseFloat("1.5", f));
    EXPECT_FLOAT_EQ(f, 1.5f);
    EXPECT_TRUE(parseFloat("-2e-1", f));
    EXPECT_FLOAT_EQ(f, -0.2f);
    EXPECT_TRUE(parseInt("-7", i));
    EXPECT_EQ(i, -7);
    EXPECT_FALSE(parseFloat("", f));
    EXPECT_FALSE(parseFloat("abc", f));
    EXPECT_FALSE(parseInt("x", i));
    EXPECT_FALSE(parseInt("", i));
}

TEST(CommandParseTest, ParseFloatsRangeAndFailure)
{
    const std::vector<std::string> parts = {"CreateSphere", "1", "2", "3", "x", "5"};
    float out[3];
    EXPECT_TRUE(parseFloats(parts, 1, 3, out));
    EXPECT_FLOAT_EQ(out[2], 3.f);
    EXPECT_FALSE(parseFloats(parts, 2, 3, out));  // contains "x"
    EXPECT_FALSE(parseFloats(parts, 4, 3, out));  // runs past the end
}

TEST(CommandParseTest, ParseIntsRange)
{
    const std::vector<std::string> parts = {"c", "4", "5", "6"};
    int out[3];
    EXPECT_TRUE(parseInts(parts, 1, 3, out));
    EXPECT_EQ(out[0], 4);
    EXPECT_EQ(out[2], 6);
    EXPECT_FALSE(parseInts(parts, 2, 3, out));
}

TEST(CommandParseTest, ScalarCommandLookupRequiresNameAndArity)
{
    const auto* spec = findScalarCommand({"SetPBVRPhaseG", "0.3"});
    ASSERT_NE(spec, nullptr);
    EXPECT_EQ(spec->kind, ScalarArg::Float);
    EXPECT_EQ(findScalarCommand({"SetPBVRPhaseG"}), nullptr);              // missing value
    EXPECT_EQ(findScalarCommand({"SetPBVRPhaseG", "1", "2"}), nullptr);    // extra value
    EXPECT_EQ(findScalarCommand({"SetPBVRNoSuchThing", "1"}), nullptr);
    EXPECT_EQ(findScalarCommand({}), nullptr);
}

TEST(CommandParseTest, ScalarValueKinds)
{
    ScalarValue v;
    const auto* f = findScalarCommand({"SetPBVRExtinction", "x"});
    ASSERT_NE(f, nullptr);
    EXPECT_FALSE(parseScalarValue(*f, "x", v));
    EXPECT_TRUE(parseScalarValue(*f, "2.5", v));
    EXPECT_FLOAT_EQ(v.f, 2.5f);

    const auto* n = findScalarCommand({"SetPBVRProbeCount", "3"});
    ASSERT_NE(n, nullptr);
    EXPECT_FALSE(parseScalarValue(*n, "", v));
    EXPECT_TRUE(parseScalarValue(*n, "12", v));
    EXPECT_EQ(v.i, 12);

    const auto* b = findScalarCommand({"SetPBVRShadowEnabled", "0"});
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(parseScalarValue(*b, "0", v));
    EXPECT_FALSE(v.flag);
    EXPECT_TRUE(parseScalarValue(*b, "1", v));
    EXPECT_TRUE(v.flag);
    EXPECT_TRUE(parseScalarValue(*b, "anything", v));  // only exactly "0" is false
    EXPECT_TRUE(v.flag);
}

TEST(CommandParseTest, ScalarTableNamesAreUniqueAndPrefixed)
{
    const auto& cmds = pbvrScalarCommands();
    for (size_t i = 0; i < cmds.size(); ++i) {
        EXPECT_EQ(std::string(cmds[i].name).rfind("SetPBVR", 0), 0u) << cmds[i].name;
        for (size_t j = i + 1; j < cmds.size(); ++j)
            EXPECT_STRNE(cmds[i].name, cmds[j].name);
    }
}
