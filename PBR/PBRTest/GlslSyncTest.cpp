#include <gtest/gtest.h>
#include "../Core/LightBlock.h"
#include "PBR/LightGlslSource.h"   // generated from PBR/Shaders/*.glsl

#include <string>

using namespace Phantom::PBR;

// The shared GLSL declares the LightBlock members; this pins the member order and array sizes to the C++ struct so the two cannot
// drift apart silently (LightBlockTest pins the C++ offsets).
TEST(GlslSyncTest, BlockMembersAppearInTheOrderOfTheCppStruct) {
	const std::string glsl = kLightBlockMembersGlsl;
	const std::string size = "[" + std::to_string(kMaxViewportLights) + "]";
	const std::string ordered[] = {
		"vec4 ambient;", "vec4 lightInfo;", "vec4 lightVec" + size + ";", "vec4 lightColor" + size + ";", "vec4 lightDir" + size + ";",
		"mat4 shadowVP;", "vec4 shadowInfo;", "vec4 areaU" + size + ";", "vec4 areaV" + size + ";" };
	size_t at = 0;
	for (const std::string& m : ordered) {
		const size_t found = glsl.find(m, at);
		ASSERT_NE(std::string::npos, found) << m;
		at = found + m.size();
	}
}

TEST(GlslSyncTest, LightFunctionsAreEmbedded) {
	const std::string eval = kLightEvalGlsl;
	for (const char* fn : { "float shadowVisibility(", "void evalLight(", "vec3 lightIrradiance(" })
		EXPECT_NE(std::string::npos, eval.find(fn)) << fn;
}
