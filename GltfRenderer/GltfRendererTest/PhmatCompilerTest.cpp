#include "gtest/gtest.h"

#include "../Phmat/PhmatCompiler.h"
#include "../Phmat/PhmatGraph.h"

#include <filesystem>
#include <fstream>

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
    diags.clear();
    ASSERT_TRUE(compileGraphToGlsl(graph, order, glsl, diags)) << (diags.empty() ? "" : diags[0].message);

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
    diags.clear();
    ASSERT_TRUE(compileGraphToGlsl(graph, order, glsl, diags));

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
}
