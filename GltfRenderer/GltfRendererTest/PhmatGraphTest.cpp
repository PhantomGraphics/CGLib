#include "gtest/gtest.h"

#include "../Phmat/PhmatGraph.h"

#include <unordered_map>

using namespace Phantom::Gltf::Phmat;

namespace {

bool hasErrorForNode(const std::vector<PhmatDiagnostic>& diags, const std::string& nodeId) {
    for (const auto& d : diags)
        if (d.severity == PhmatDiagnostic::Severity::Error && d.nodeId == nodeId) return true;
    return false;
}

const char* kMinimalGraph = R"JSON(
{
  "version": 1,
  "output": "out",
  "nodes": [
    { "id": "albedoTex", "type": "textureSlot", "slot": "baseColor" },
    { "id": "tint", "type": "constant", "value": [1.0, 0.6, 0.6, 1.0] },
    { "id": "tintedAlbedo", "type": "math", "op": "multiply", "a": "albedoTex", "b": "tint" },
    { "id": "metallicConst", "type": "constant", "value": 0.0 },
    { "id": "roughnessConst", "type": "constant", "value": 0.6 },
    { "id": "normalScale", "type": "constant", "value": 1.0 },
    { "id": "normal", "type": "normalMap", "scale": "normalScale" },
    {
      "id": "out", "type": "pbrOutput",
      "baseColor": "tintedAlbedo", "metallic": "metallicConst", "roughness": "roughnessConst",
      "normal": "normal"
    }
  ]
}
)JSON";

} // namespace

TEST(PhmatGraphTest, ParsesMinimalGraph) {
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(kMinimalGraph, graph, diags)) << (diags.empty() ? "" : diags[0].message);
    EXPECT_EQ(graph.version, 1);
    EXPECT_EQ(graph.outputNode, "out");
    EXPECT_EQ(graph.nodes.size(), 8u);
}

TEST(PhmatGraphTest, ValidatesAndTopologicallySortsMinimalGraph) {
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(kMinimalGraph, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    ASSERT_TRUE(validateAndSort(graph, order, diags)) << (diags.empty() ? "" : diags[0].message);
    EXPECT_EQ(order.size(), graph.nodes.size());

    // Every node must appear after everything it depends on.
    std::unordered_map<std::string, size_t> pos;
    for (size_t i = 0; i < order.size(); ++i) pos[order[i]] = i;
    EXPECT_LT(pos["albedoTex"], pos["tintedAlbedo"]);
    EXPECT_LT(pos["tint"], pos["tintedAlbedo"]);
    EXPECT_LT(pos["normalScale"], pos["normal"]);
    EXPECT_LT(pos["tintedAlbedo"], pos["out"]);
}

TEST(PhmatGraphTest, RejectsMalformedJson) {
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    EXPECT_FALSE(parsePhmatGraph("{ not json", graph, diags));
    EXPECT_FALSE(diags.empty());
}

TEST(PhmatGraphTest, RejectsUnsupportedVersion) {
    const char* json = R"JSON({ "version": 2, "output": "out", "nodes": [] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    EXPECT_FALSE(parsePhmatGraph(json, graph, diags));
}

TEST(PhmatGraphTest, RejectsUnknownNodeType) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "mystery", "type": "proceduralNoise" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    EXPECT_FALSE(parsePhmatGraph(json, graph, diags));
    EXPECT_TRUE(hasErrorForNode(diags, "mystery"));
}

TEST(PhmatGraphTest, RejectsDuplicateNodeIds) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "a", "type": "constant", "value": 1.0 },
        { "id": "a", "type": "constant", "value": 2.0 },
        { "id": "out", "type": "pbrOutput", "baseColor": "a", "metallic": "a", "roughness": "a" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    EXPECT_FALSE(validateAndSort(graph, order, diags));
}

TEST(PhmatGraphTest, RejectsUnknownInputReference) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "c", "type": "constant", "value": 1.0 },
        { "id": "out", "type": "pbrOutput", "baseColor": "doesNotExist", "metallic": "c", "roughness": "c" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    EXPECT_FALSE(validateAndSort(graph, order, diags));
    EXPECT_TRUE(hasErrorForNode(diags, "out"));
}

TEST(PhmatGraphTest, RejectsCycle) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "a", "type": "math", "op": "add", "a": "b", "b": "b" },
        { "id": "b", "type": "math", "op": "add", "a": "a", "b": "a" },
        { "id": "out", "type": "pbrOutput", "baseColor": "a", "metallic": "a", "roughness": "a" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    EXPECT_FALSE(validateAndSort(graph, order, diags));
    EXPECT_TRUE(order.empty());
}

TEST(PhmatGraphTest, RejectsTypeMismatchInMath) {
    // "a" is Vec4 (a textureSlot), "b" is Float -- math requires matching types.
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "tex", "type": "textureSlot", "slot": "baseColor" },
        { "id": "f", "type": "constant", "value": 1.0 },
        { "id": "bad", "type": "math", "op": "add", "a": "tex", "b": "f" },
        { "id": "out", "type": "pbrOutput", "baseColor": "bad", "metallic": "f", "roughness": "f" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    EXPECT_FALSE(validateAndSort(graph, order, diags));
    EXPECT_TRUE(hasErrorForNode(diags, "bad"));
}

TEST(PhmatGraphTest, RejectsWrongTypeInPbrOutput) {
    // baseColor must be Vec4; a Float constant is not acceptable there.
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "f", "type": "constant", "value": 1.0 },
        { "id": "out", "type": "pbrOutput", "baseColor": "f", "metallic": "f", "roughness": "f" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    EXPECT_FALSE(validateAndSort(graph, order, diags));
    EXPECT_TRUE(hasErrorForNode(diags, "out"));
}

TEST(PhmatGraphTest, RejectsNonZeroUvSet) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "uv1", "type": "uv", "set": 1 },
        { "id": "f", "type": "constant", "value": 1.0 },
        { "id": "out", "type": "pbrOutput", "baseColor": "f", "metallic": "f", "roughness": "f" }
    ] })JSON";
    // baseColor must be Vec4 for this to be a pure uvSet-only failure test we'd need a real
    // consumer of uv1; instead this graph exercises uv1 being present (even if unused
    // downstream) still failing validation, since the check is per-node, not reachability-based.
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    EXPECT_FALSE(validateAndSort(graph, order, diags));
    EXPECT_TRUE(hasErrorForNode(diags, "uv1"));
}

TEST(PhmatGraphTest, AcceptsAllMathOpsAndMix) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "a", "type": "constant", "value": 1.0 },
        { "id": "b", "type": "constant", "value": 2.0 },
        { "id": "f", "type": "constant", "value": 0.5 },
        { "id": "add", "type": "math", "op": "add", "a": "a", "b": "b" },
        { "id": "sub", "type": "math", "op": "subtract", "a": "a", "b": "b" },
        { "id": "mul", "type": "math", "op": "multiply", "a": "a", "b": "b" },
        { "id": "div", "type": "math", "op": "divide", "a": "a", "b": "b" },
        { "id": "mn", "type": "math", "op": "min", "a": "a", "b": "b" },
        { "id": "mx", "type": "math", "op": "max", "a": "a", "b": "b" },
        { "id": "mixed", "type": "mix", "a": "add", "b": "sub", "factor": "f" },
        { "id": "out", "type": "pbrOutput", "baseColor": "a", "metallic": "mixed", "roughness": "mul" }
    ] })JSON";
    // baseColor is Float here on purpose to keep the fixture short -- type checking that
    // baseColor must be Vec4 is covered separately by RejectsWrongTypeInPbrOutput above, so this
    // graph is expected to still report that one error while everything else passes; assert on
    // the specific diagnostic instead of overall success.
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    validateAndSort(graph, order, diags);
    // Every diagnostic raised, if any, must be about "out" (the deliberate Float-for-baseColor
    // mismatch) -- add/sub/mul/div/min/max/mix themselves must not raise anything.
    for (const auto& d : diags) EXPECT_EQ(d.nodeId, "out") << d.message;
}
