#include "gtest/gtest.h"

#include "../Phmat/PhmatCompiler.h"
#include "../Phmat/PhmatGraph.h"
#include "../Phmat/PhmatReflection.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

using namespace Phantom::Gltf::Phmat;

namespace {

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
    { "id": "emissiveTint", "type": "constant", "value": [0.0, 0.0, 0.0] },
    {
      "id": "out", "type": "pbrOutput",
      "baseColor": "tintedAlbedo", "metallic": "metallicConst", "roughness": "roughnessConst",
      "normal": "normal", "emissive": "emissiveTint"
    }
  ]
}
)JSON";

std::filesystem::path writeTempFile(const char* name, const char* text) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return path;
}

} // namespace

TEST(PhmatCompilerTest, GeneratesGlslWithExpectedShape) {
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(kMinimalGraph, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    ASSERT_TRUE(validateAndSort(graph, order, diags));

    std::string glsl;
    std::vector<PhshaderSplice> splices;
    diags.clear();
    ASSERT_TRUE(compileGraphToGlsl(graph, order, {}, glsl, splices, diags)) << (diags.empty() ? "" : diags[0].message);
    EXPECT_TRUE(splices.empty()); // no Custom nodes in this fixture

    // Header: same descriptor/vertex-input interface as gltf.frag, minus MaterialUBO.
    EXPECT_NE(glsl.find("#version 450"), std::string::npos);
    EXPECT_NE(glsl.find("layout(set = 0, binding = 0) uniform GlobalUBO"), std::string::npos);
    EXPECT_EQ(glsl.find("uniform MaterialUBO"), std::string::npos); // deliberately not declared
    EXPECT_NE(glsl.find("layout(set = 1, binding = 1) uniform sampler2D baseColorTex"), std::string::npos);
    EXPECT_NE(glsl.find("layout(location = 5) in vec4 fragPosLightSpace"), std::string::npos);

    // Generated node declarations (variable names are "v_" + sanitized node id).
    EXPECT_NE(glsl.find("vec4 v_albedoTex = texture(baseColorTex, fragTexCoord);"), std::string::npos);
    EXPECT_NE(glsl.find("vec4 v_tint = vec4("), std::string::npos);
    EXPECT_NE(glsl.find("v_tintedAlbedo = v_albedoTex * v_tint;"), std::string::npos);
    EXPECT_NE(glsl.find("tn.xy *= v_normalScale;"), std::string::npos);

    // pbrOutput wiring into the shared lighting tail.
    EXPECT_NE(glsl.find("vec4 baseColor = v_tintedAlbedo;"), std::string::npos);
    EXPECT_NE(glsl.find("float metallic  = clamp(v_metallicConst, 0.0, 1.0);"), std::string::npos);
    EXPECT_NE(glsl.find("vec3 N = v_normal;"), std::string::npos);
    EXPECT_NE(glsl.find("float occlusion = 1.0;"), std::string::npos); // optional, left unset in the fixture
    EXPECT_NE(glsl.find("vec3 emissive = v_emissiveTint;"), std::string::npos);
    EXPECT_NE(glsl.find("outColor = vec4(color, alpha);"), std::string::npos);
}

TEST(PhmatCompilerTest, DefaultsForOptionalPbrOutputFieldsMatchFixedShader) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc", "type": "constant", "value": [1.0, 1.0, 1.0, 1.0] },
        { "id": "f",  "type": "constant", "value": 0.5 },
        { "id": "out", "type": "pbrOutput", "baseColor": "bc", "metallic": "f", "roughness": "f" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    ASSERT_TRUE(validateAndSort(graph, order, diags));

    std::string glsl;
    std::vector<PhshaderSplice> splices;
    diags.clear();
    ASSERT_TRUE(compileGraphToGlsl(graph, order, {}, glsl, splices, diags));

    EXPECT_NE(glsl.find("vec3 N = normalize(fragNormal);"), std::string::npos);
    EXPECT_NE(glsl.find("float occlusion = 1.0;"), std::string::npos);
    EXPECT_NE(glsl.find("vec3 emissive = vec3(0.0);"), std::string::npos);
    EXPECT_NE(glsl.find("float alpha = baseColor.a;"), std::string::npos);
}

// Requires the Vulkan SDK's glslc -- degrades to a skip (not a failure) when unavailable, same
// convention every other Vulkan-dependent test in this repo follows when the SDK is missing.
TEST(PhmatCompilerTest, LoadPhmatMaterialCompilesEndToEndWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping end-to-end compile test";
    }

    const auto phmatPath = writeTempFile("phmat_compiler_test.phmat", kMinimalGraph);
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    ASSERT_TRUE(result.success) << (result.diagnostics.empty() ? "" : result.diagnostics[0].message);
    EXPECT_FALSE(result.fragSpirv.empty());
    // SPIR-V is a binary word stream; a real module starts with the fixed magic number 0x07230203.
    EXPECT_EQ(result.fragSpirv.front(), 0x07230203u);

    // Second call should hit the cache (same generated GLSL -> same content hash -> same .spv
    // read straight back without invoking glslc again); result must be byte-identical either way.
    PhmatLoadResult second = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    ASSERT_TRUE(second.success);
    EXPECT_EQ(result.fragSpirv, second.fragSpirv);
}

TEST(PhmatCompilerTest, LoadPhmatMaterialReportsParseErrorsWithoutInvokingGlslc) {
    const auto phmatPath = writeTempFile("phmat_compiler_test_bad.phmat", "{ not json");
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache_bad";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.fragSpirv.empty());
    EXPECT_FALSE(result.diagnostics.empty());

    // Phase 4C item 5 ("hot reload"): dependencyPaths must at least name the .phmat file itself
    // even when it never got far enough to discover any .phshader references, so a caller
    // watching for edits still notices a fix to this exact file.
    ASSERT_EQ(result.dependencyPaths.size(), 1u);
    EXPECT_EQ(result.dependencyPaths[0], phmatPath.string());
}

// ============================================================
//  Custom node / ".phshader" (Phase 4C item 4)
// ============================================================

TEST(PhmatCompilerTest, CustomNodeGeneratesFunctionDeclarationAndCallSite) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc",  "type": "constant", "value": [1.0, 0.5, 0.5, 1.0] },
        { "id": "amt", "type": "constant", "value": 0.5 },
        { "id": "tinted", "type": "custom", "phshader": "tint.phshader", "output": "vec4",
          "inputs": [ { "id": "bc", "type": "vec4" }, { "id": "amt", "type": "float" } ] },
        { "id": "f", "type": "constant", "value": 0.3 },
        { "id": "out", "type": "pbrOutput", "baseColor": "tinted", "metallic": "f", "roughness": "f" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags)) << (diags.empty() ? "" : diags[0].message);

    std::vector<std::string> order;
    diags.clear();
    ASSERT_TRUE(validateAndSort(graph, order, diags)) << (diags.empty() ? "" : diags[0].message);

    PhshaderSource src;
    src.version = 1;
    src.functionName = "tintPulse";
    src.inputs = { { "baseColor", ValueType::Vec4 }, { "amount", ValueType::Float } };
    src.outputType = ValueType::Vec4;
    src.body = "return mix(baseColor, vec4(1.0), amount);";
    std::unordered_map<std::string, PhshaderSource> phshaders = { { "tinted", src } };

    std::string glsl;
    std::vector<PhshaderSplice> splices;
    diags.clear();
    ASSERT_TRUE(compileGraphToGlsl(graph, order, phshaders, glsl, splices, diags)) << (diags.empty() ? "" : diags[0].message);

    EXPECT_NE(glsl.find("vec4 tintPulse(vec4 baseColor, float amount) {"), std::string::npos);
    EXPECT_NE(glsl.find("return mix(baseColor, vec4(1.0), amount);"), std::string::npos);
    EXPECT_NE(glsl.find("vec4 v_tinted = tintPulse(v_bc, v_amt);"), std::string::npos);

    ASSERT_EQ(splices.size(), 1u);
    EXPECT_EQ(splices[0].nodeId, "tinted");
    EXPECT_EQ(splices[0].phshaderPath, "tint.phshader");
    EXPECT_EQ(splices[0].firstBodyLineInGenerated, splices[0].lastBodyLineInGenerated); // single-line body

    // The recorded line number must actually point at the body's own line within outGlsl.
    std::istringstream lines(glsl);
    std::string line;
    int lineNo = 0;
    std::string atSplice;
    while (std::getline(lines, line)) {
        if (++lineNo == splices[0].firstBodyLineInGenerated) { atSplice = line; break; }
    }
    EXPECT_EQ(atSplice, "return mix(baseColor, vec4(1.0), amount);");
}

TEST(PhmatCompilerTest, CustomNodeSignatureMismatchAgainstPhshaderIsReported) {
    const char* json = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "f", "type": "constant", "value": 0.5 },
        { "id": "bad", "type": "custom", "phshader": "x.phshader", "output": "float", "inputs": [] },
        { "id": "bc", "type": "constant", "value": [1.0, 1.0, 1.0, 1.0] },
        { "id": "out", "type": "pbrOutput", "baseColor": "bc", "metallic": "bad", "roughness": "f" }
    ] })JSON";
    PhmatGraph graph;
    std::vector<PhmatDiagnostic> diags;
    ASSERT_TRUE(parsePhmatGraph(json, graph, diags));

    std::vector<std::string> order;
    diags.clear();
    ASSERT_TRUE(validateAndSort(graph, order, diags)) << (diags.empty() ? "" : diags[0].message);

    // The node declares "float", but this x.phshader source declares "vec4" -- a mismatch that
    // parsePhmatGraph()/validateAndSort() cannot catch on their own (they never touch the
    // filesystem), so compileGraphToGlsl() must catch it instead.
    PhshaderSource src;
    src.version = 1;
    src.functionName = "wrongType";
    src.outputType = ValueType::Vec4;
    src.body = "return vec4(1.0);";
    std::unordered_map<std::string, PhshaderSource> phshaders = { { "bad", src } };

    std::string glsl;
    std::vector<PhshaderSplice> splices;
    diags.clear();
    EXPECT_FALSE(compileGraphToGlsl(graph, order, phshaders, glsl, splices, diags));
    bool found = false;
    for (const auto& d : diags) if (d.nodeId == "bad") found = true;
    EXPECT_TRUE(found);
    EXPECT_TRUE(splices.empty()); // the mismatched node's function was never emitted
}

// Requires the Vulkan SDK's glslc -- degrades to a skip (not a failure) when unavailable, same
// convention every other Vulkan-dependent test in this repo follows when the SDK is missing.
TEST(PhmatCompilerTest, LoadPhmatMaterialCompilesCustomNodeEndToEndWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping end-to-end compile test";
    }

    const char* phshaderJson = R"JSON(
    { "version": 1, "functionName": "tintPulse", "output": "vec4",
      "inputs": [ { "name": "baseColor", "type": "vec4" }, { "name": "amount", "type": "float" } ],
      "body": "return mix(baseColor, vec4(1.0), amount);" })JSON";
    writeTempFile("phmat_compiler_test_tint.phshader", phshaderJson);

    const char* phmatJson = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc",  "type": "constant", "value": [1.0, 0.5, 0.5, 1.0] },
        { "id": "amt", "type": "constant", "value": 0.5 },
        { "id": "tinted", "type": "custom", "phshader": "phmat_compiler_test_tint.phshader", "output": "vec4",
          "inputs": [ { "id": "bc", "type": "vec4" }, { "id": "amt", "type": "float" } ] },
        { "id": "f", "type": "constant", "value": 0.3 },
        { "id": "out", "type": "pbrOutput", "baseColor": "tinted", "metallic": "f", "roughness": "f" }
    ] })JSON";
    const auto phmatPath = writeTempFile("phmat_compiler_test_custom.phmat", phmatJson);
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache_custom";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    ASSERT_TRUE(result.success) << (result.diagnostics.empty() ? "" : result.diagnostics[0].message);
    EXPECT_FALSE(result.fragSpirv.empty());
    EXPECT_EQ(result.fragSpirv.front(), 0x07230203u);

    // Phase 4C item 5 ("hot reload"): the .phmat itself plus the one .phshader it referenced,
    // resolved relative to the .phmat's own directory (loadPhmatMaterial()'s existing convention).
    const auto expectedPhshaderPath = (phmatPath.parent_path() / "phmat_compiler_test_tint.phshader").string();
    ASSERT_EQ(result.dependencyPaths.size(), 2u);
    EXPECT_EQ(result.dependencyPaths[0], phmatPath.string());
    EXPECT_EQ(result.dependencyPaths[1], expectedPhshaderPath);
}

TEST(PhmatCompilerTest, LoadPhmatMaterialDependencyPathsDeduplicateASharedPhshaderWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping end-to-end compile test";
    }

    const char* phshaderJson = R"JSON(
    { "version": 1, "functionName": "identityFn", "output": "float",
      "inputs": [ { "name": "x", "type": "float" } ], "body": "return x;" })JSON";
    writeTempFile("phmat_compiler_test_shared.phshader", phshaderJson);

    // Two distinct Custom nodes both reference the same .phshader file.
    const char* phmatJson = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc", "type": "constant", "value": [1.0, 1.0, 1.0, 1.0] },
        { "id": "a",  "type": "constant", "value": 0.5 },
        { "id": "b",  "type": "constant", "value": 0.6 },
        { "id": "m1", "type": "custom", "phshader": "phmat_compiler_test_shared.phshader", "output": "float",
          "inputs": [ { "id": "a", "type": "float" } ] },
        { "id": "m2", "type": "custom", "phshader": "phmat_compiler_test_shared.phshader", "output": "float",
          "inputs": [ { "id": "b", "type": "float" } ] },
        { "id": "out", "type": "pbrOutput", "baseColor": "bc", "metallic": "m1", "roughness": "m2" }
    ] })JSON";
    const auto phmatPath = writeTempFile("phmat_compiler_test_shared.phmat", phmatJson);
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache_shared";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    ASSERT_TRUE(result.success) << (result.diagnostics.empty() ? "" : result.diagnostics[0].message);

    // Exactly 2 -- the .phmat itself and the one shared .phshader, not 3 (once per referencing node).
    const auto expectedPhshaderPath = (phmatPath.parent_path() / "phmat_compiler_test_shared.phshader").string();
    ASSERT_EQ(result.dependencyPaths.size(), 2u);
    EXPECT_EQ(result.dependencyPaths[0], phmatPath.string());
    EXPECT_EQ(result.dependencyPaths[1], expectedPhshaderPath);
}

TEST(PhmatCompilerTest, LoadPhmatMaterialRejectsIllegalDescriptorBindingViaReflectionWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping reflection compile test";
    }

    const char* phshaderJson = R"JSON(
    { "version": 1, "functionName": "illegalRead", "output": "float", "inputs": [],
      "helpers": "layout(set = 2, binding = 0) uniform sampler2D evilTex;",
      "body": "return texture(evilTex, vec2(0.5)).r;" })JSON";
    writeTempFile("phmat_compiler_test_illegal.phshader", phshaderJson);

    const char* phmatJson = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc",  "type": "constant", "value": [1.0, 1.0, 1.0, 1.0] },
        { "id": "bad", "type": "custom", "phshader": "phmat_compiler_test_illegal.phshader", "output": "float", "inputs": [] },
        { "id": "out", "type": "pbrOutput", "baseColor": "bc", "metallic": "bad", "roughness": "bad" }
    ] })JSON";
    const auto phmatPath = writeTempFile("phmat_compiler_test_illegal_descriptor.phmat", phmatJson);
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache_illegal_descriptor";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.diagnostics.empty());
    bool foundDescriptorComplaint = false;
    for (const auto& d : result.diagnostics)
        if (d.message.find("descriptor binding") != std::string::npos) foundDescriptorComplaint = true;
    EXPECT_TRUE(foundDescriptorComplaint);
}

TEST(PhmatCompilerTest, LoadPhmatMaterialRejectsPushConstantsViaReflectionWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping reflection compile test";
    }

    const char* phshaderJson = R"JSON(
    { "version": 1, "functionName": "readPushConstant", "output": "float", "inputs": [],
      "helpers": "layout(push_constant) uniform PC { float x; } pc;",
      "body": "return pc.x;" })JSON";
    writeTempFile("phmat_compiler_test_pushconst.phshader", phshaderJson);

    const char* phmatJson = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc",  "type": "constant", "value": [1.0, 1.0, 1.0, 1.0] },
        { "id": "bad", "type": "custom", "phshader": "phmat_compiler_test_pushconst.phshader", "output": "float", "inputs": [] },
        { "id": "out", "type": "pbrOutput", "baseColor": "bc", "metallic": "bad", "roughness": "bad" }
    ] })JSON";
    const auto phmatPath = writeTempFile("phmat_compiler_test_pushconst.phmat", phmatJson);
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache_pushconst";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.diagnostics.empty());
    bool foundPushConstantComplaint = false;
    for (const auto& d : result.diagnostics)
        if (d.message.find("push constant") != std::string::npos) foundPushConstantComplaint = true;
    EXPECT_TRUE(foundPushConstantComplaint);
}

TEST(PhmatCompilerTest, LoadPhmatMaterialMapsCompileErrorBackToPhshaderLineWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping error-location test";
    }

    // Deliberate syntax error on the body's own second line.
    const char* phshaderJson = R"JSON(
    { "version": 1, "functionName": "brokenFn", "output": "float",
      "inputs": [ { "name": "x", "type": "float" } ],
      "body": "float y = x * 2.0;\nreturn y + ;" })JSON";
    writeTempFile("phmat_compiler_test_broken.phshader", phshaderJson);

    const char* phmatJson = R"JSON(
    { "version": 1, "output": "out", "nodes": [
        { "id": "bc", "type": "constant", "value": [1.0, 1.0, 1.0, 1.0] },
        { "id": "f",  "type": "constant", "value": 0.5 },
        { "id": "bad", "type": "custom", "phshader": "phmat_compiler_test_broken.phshader", "output": "float",
          "inputs": [ { "id": "f", "type": "float" } ] },
        { "id": "out", "type": "pbrOutput", "baseColor": "bc", "metallic": "bad", "roughness": "f" }
    ] })JSON";
    const auto phmatPath = writeTempFile("phmat_compiler_test_broken_custom.phmat", phmatJson);
    const auto cacheDir  = std::filesystem::temp_directory_path() / "phmat_compiler_test_cache_broken_custom";

    PhmatLoadResult result = loadPhmatMaterial(phmatPath.string(), cacheDir.string());
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.diagnostics.empty());

    bool found = false;
    std::string allMessages;
    for (const auto& d : result.diagnostics) {
        allMessages += "[" + d.nodeId + "] " + d.message + "\n";
        if (d.nodeId == "bad" && d.message.find("phmat_compiler_test_broken.phshader:2:") != std::string::npos) found = true;
    }
    EXPECT_TRUE(found) << allMessages;
}
