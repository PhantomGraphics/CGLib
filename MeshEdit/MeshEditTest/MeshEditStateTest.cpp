#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "../Core/MeshEditState.h"

using namespace Phantom::MeshEdit;

namespace {

// 8 vertices (ids 0..7), 12 edges, 6 faces.
const EditableMesh& box() {
    static const EditableMesh m = makeEditableBox();
    return m;
}

}  // namespace

TEST(MeshEditState, SelectModeNamesAreCaseInsensitive) {
    SelectMode m = SelectMode::Toggle;
    EXPECT_TRUE(selectModeFromName("REPLACE", m)); EXPECT_EQ(SelectMode::Replace, m);
    EXPECT_TRUE(selectModeFromName("Add", m));     EXPECT_EQ(SelectMode::Add, m);
    EXPECT_TRUE(selectModeFromName("toggle", m));  EXPECT_EQ(SelectMode::Toggle, m);
    EXPECT_FALSE(selectModeFromName("subtract", m));
}

TEST(MeshEditState, ReplaceAddAndToggleCombineWithTheCurrentSelection) {
    MeshEditState s;
    s.applySelection(ElementKind::Vertex, { 3, 1 }, SelectMode::Replace, false, true);
    EXPECT_EQ((std::vector<uint32_t>{ 1, 3 }), s.ids);            // kept ascending
    s.applySelection(ElementKind::Vertex, { 2, 3 }, SelectMode::Add, false, true);
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2, 3 }), s.ids);
    s.applySelection(ElementKind::Vertex, { 3, 5 }, SelectMode::Toggle, false, true);
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2, 5 }), s.ids);          // 3 leaves, 5 joins
    s.applySelection(ElementKind::Vertex, { 7 }, SelectMode::Replace, false, true);
    EXPECT_EQ((std::vector<uint32_t>{ 7 }), s.ids);
}

TEST(MeshEditState, ReplaceWithoutExplicitIdsKeepsTheSelectionAndClearEmptiesIt) {
    MeshEditState s;
    s.applySelection(ElementKind::Face, { 1, 2 }, SelectMode::Replace, false, true);
    s.applySelection(ElementKind::Face, {}, SelectMode::Replace, false, false);   // "mesh select face" with no ids=
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2 }), s.ids);
    s.applySelection(ElementKind::Face, {}, SelectMode::Replace, false, true);    // an explicit empty request replaces
    EXPECT_TRUE(s.ids.empty());
    s.applySelection(ElementKind::Face, { 4 }, SelectMode::Replace, false, true);
    s.applySelection(ElementKind::Face, {}, SelectMode::Add, true, false);        // clear wins
    EXPECT_TRUE(s.ids.empty());
}

TEST(MeshEditState, SwitchingTheElementKindStartsAFreshSelection) {
    MeshEditState s;
    s.applySelection(ElementKind::Vertex, { 1, 2 }, SelectMode::Replace, false, true);
    s.applySelection(ElementKind::Edge, { 9 }, SelectMode::Add, false, true);
    EXPECT_EQ(ElementKind::Edge, s.kind);
    EXPECT_EQ((std::vector<uint32_t>{ 9 }), s.ids);                // vertex ids are meaningless as edges
}

TEST(MeshEditState, ElementIdListsAcceptIdsRangesAllAndNone) {
    ElementIdRequest r; MeshArgError e;
    ASSERT_TRUE(parseElementIds("0,2,4-6", box(), ElementKind::Vertex, r, e)) << e.message;
    EXPECT_EQ((std::vector<uint32_t>{ 0, 2, 4, 5, 6 }), r.ids);
    EXPECT_FALSE(r.clear);
    ASSERT_TRUE(parseElementIds("all", box(), ElementKind::Face, r, e));
    EXPECT_EQ(6u, r.ids.size());
    ASSERT_TRUE(parseElementIds("ALL", box(), ElementKind::Vertex, r, e));
    EXPECT_EQ(8u, r.ids.size());
    ASSERT_TRUE(parseElementIds("none", box(), ElementKind::Vertex, r, e));
    EXPECT_TRUE(r.clear); EXPECT_TRUE(r.ids.empty());
    ASSERT_TRUE(parseElementIds("", box(), ElementKind::Vertex, r, e));
    EXPECT_TRUE(r.clear);
}

TEST(MeshEditState, ElementIdListsRejectMalformedAndMissingIds) {
    ElementIdRequest r; MeshArgError e;
    EXPECT_FALSE(parseElementIds("1,x", box(), ElementKind::Vertex, r, e));
    EXPECT_EQ("invalid_argument", e.code);
    EXPECT_FALSE(parseElementIds("5-2", box(), ElementKind::Vertex, r, e));      // backwards range
    EXPECT_EQ("invalid_argument", e.code);
    EXPECT_FALSE(parseElementIds("-3", box(), ElementKind::Vertex, r, e));       // negative id
    EXPECT_EQ("invalid_argument", e.code);
    EXPECT_FALSE(parseElementIds("0-20000000", box(), ElementKind::Vertex, r, e));  // absurd range
    EXPECT_EQ("invalid_argument", e.code);
    EXPECT_FALSE(parseElementIds("0,99", box(), ElementKind::Vertex, r, e));
    EXPECT_EQ("not_found", e.code);
    EXPECT_NE(std::string::npos, e.message.find("99"));
    EXPECT_FALSE(parseElementIds("0-8", box(), ElementKind::Vertex, r, e));      // 8 does not exist (ids 0..7)
    EXPECT_EQ("not_found", e.code);
}

TEST(MeshEditState, NumberListsNeedExactlyTheRequestedCountOfFiniteNumbers) {
    std::vector<double> v; MeshArgError e;
    ASSERT_TRUE(parseNumberList("10.5,20", 2, v, e));
    EXPECT_EQ((std::vector<double>{ 10.5, 20.0 }), v);
    ASSERT_TRUE(parseNumberList("1,2,3,4", 4, v, e));
    EXPECT_EQ(4u, v.size());
    EXPECT_FALSE(parseNumberList("1,2,3", 2, v, e));
    EXPECT_FALSE(parseNumberList("1,", 2, v, e));
    EXPECT_FALSE(parseNumberList("1,abc", 2, v, e));
    EXPECT_FALSE(parseNumberList("1,nan", 2, v, e));
    EXPECT_FALSE(parseNumberList("1,inf", 2, v, e));
    EXPECT_EQ("invalid_argument", e.code);
}

TEST(MeshEditState, MirrorAxesAcceptCombinationsAndDefaultToX) {
    std::array<bool, 3> a; MeshArgError e;
    ASSERT_TRUE(parseMirrorAxes("", a, e));
    EXPECT_EQ((std::array<bool, 3>{ true, false, false }), a);
    ASSERT_TRUE(parseMirrorAxes("xy", a, e));
    EXPECT_EQ((std::array<bool, 3>{ true, true, false }), a);
    ASSERT_TRUE(parseMirrorAxes("Z", a, e));
    EXPECT_EQ((std::array<bool, 3>{ false, false, true }), a);
    ASSERT_TRUE(parseMirrorAxes("x, y,z", a, e));
    EXPECT_EQ((std::array<bool, 3>{ true, true, true }), a);
    EXPECT_FALSE(parseMirrorAxes("w", a, e));
    EXPECT_EQ("invalid_argument", e.code);
    EXPECT_FALSE(parseMirrorAxes("xq", a, e));
}
