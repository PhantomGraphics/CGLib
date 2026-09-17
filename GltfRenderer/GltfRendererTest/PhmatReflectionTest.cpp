#include "gtest/gtest.h"

#include "../Phmat/PhmatCompiler.h" // findGlslcPath()/compileGlslToSpirv() -- to get REAL SPIR-V for one test below
#include "../Phmat/PhmatReflection.h"

#include <algorithm>
#include <filesystem>

using namespace Phantom::Gltf::Phmat;

TEST(PhmatReflectionTest, IsAllowedDescriptorBindingMatchesGltfSceneRendererLayout) {
    // set=0: GlobalUBO(0) + irradiance/prefiltered/BRDF-LUT/shadowMap(1-4) -- kGltfPbrHeader.
    EXPECT_TRUE(isAllowedDescriptorBinding({0, 0}));
    EXPECT_TRUE(isAllowedDescriptorBinding({0, 4}));
    EXPECT_FALSE(isAllowedDescriptorBinding({0, 5}));

    // set=1: MaterialUBO(0, unreferenced but valid to redeclare) + 5 texture slots(1-5).
    EXPECT_TRUE(isAllowedDescriptorBinding({1, 0}));
    EXPECT_TRUE(isAllowedDescriptorBinding({1, 5}));
    EXPECT_FALSE(isAllowedDescriptorBinding({1, 6}));

    // No other set is part of GltfSceneRenderer's fixed material pipeline layout.
    EXPECT_FALSE(isAllowedDescriptorBinding({2, 0}));
}

TEST(PhmatReflectionTest, ValidateReflectionAcceptsOnlyAllowedBindingsAndNoPushConstants) {
    PhmatReflectionResult clean;
    clean.descriptors = { {0, 0}, {1, 1}, {1, 5} };
    std::vector<std::string> messages;
    EXPECT_TRUE(validateReflection(clean, messages));
    EXPECT_TRUE(messages.empty());

    PhmatReflectionResult badBinding;
    badBinding.descriptors = { {0, 0}, {2, 0} };
    messages.clear();
    EXPECT_FALSE(validateReflection(badBinding, messages));
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_NE(messages[0].find("set=2"), std::string::npos);
    EXPECT_NE(messages[0].find("binding=0"), std::string::npos);

    PhmatReflectionResult pushConst;
    pushConst.usesPushConstants = true;
    messages.clear();
    EXPECT_FALSE(validateReflection(pushConst, messages));
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_NE(messages[0].find("push constant"), std::string::npos);

    // Both problems at once -- both messages are reported, not just the first.
    PhmatReflectionResult both;
    both.descriptors = { {3, 7} };
    both.usesPushConstants = true;
    messages.clear();
    EXPECT_FALSE(validateReflection(both, messages));
    EXPECT_EQ(messages.size(), 2u);
}

TEST(PhmatReflectionTest, ReflectSpirvRejectsInputWithoutValidMagicNumber) {
    PhmatReflectionResult result;
    EXPECT_FALSE(reflectSpirv({}, result));
    EXPECT_FALSE(reflectSpirv({0x11223344u, 0, 0, 0, 0}, result));
    EXPECT_FALSE(reflectSpirv({0x07230203u, 0, 0}, result)); // magic ok, but shorter than the 5-word header
}

// Requires the Vulkan SDK's glslc -- degrades to a skip (not a failure) when unavailable, same
// convention every other Vulkan-dependent test in this repo follows when the SDK is missing.
TEST(PhmatReflectionTest, ReflectSpirvFindsRealDescriptorBindingsWhenGlslcAvailable) {
    std::string glslcPath;
    if (!findGlslcPath(glslcPath)) {
        GTEST_SKIP() << "glslc not found (VULKAN_SDK not set); skipping real SPIR-V reflection test";
    }

    // A minimal, syntactically valid fragment shader with a handful of known bindings --
    // deliberately including one outside kGltfPbrHeader's own set/binding layout (set=1,
    // binding=1, sampler2D -- happens to be one PhmatCompiler.cpp's header *does* also declare,
    // but that overlap is irrelevant here: this test only checks reflectSpirv() itself, not the
    // pipeline-layout allowlist, which PhmatReflectionTest.ValidateReflection* already covers).
    const char* glsl = R"GLSL(#version 450
layout(set = 0, binding = 0) uniform GlobalUBO { mat4 m; } cam;
layout(set = 1, binding = 1) uniform sampler2D tex;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = texture(tex, vec2(0.0)) * cam.m[0][0];
}
)GLSL";

    const auto cacheDir = std::filesystem::temp_directory_path() / "phmat_reflection_test_cache";
    std::vector<uint32_t> spirv;
    std::string errorLog;
    ASSERT_TRUE(compileGlslToSpirv(glsl, cacheDir.string(), spirv, errorLog)) << errorLog;

    PhmatReflectionResult result;
    ASSERT_TRUE(reflectSpirv(spirv, result));
    EXPECT_FALSE(result.usesPushConstants);

    auto has = [&](uint32_t set, uint32_t binding) {
        return std::any_of(result.descriptors.begin(), result.descriptors.end(),
            [&](const PhmatDescriptorBinding& b) { return b.set == set && b.binding == binding; });
    };
    EXPECT_TRUE(has(0, 0));
    EXPECT_TRUE(has(1, 1));
}
