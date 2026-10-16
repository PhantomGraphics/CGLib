#include <gtest/gtest.h>
#include "../Core/LightBlock.h"

#include <cmath>

using namespace Phantom::PBR;

namespace {
	constexpr float kEps = 1.0e-5f;
	void expectNear(const glm::vec3& a, const glm::vec3& b) {
		EXPECT_NEAR(a.x, b.x, kEps); EXPECT_NEAR(a.y, b.y, kEps); EXPECT_NEAR(a.z, b.z, kEps);
	}
}

// The GLSL declarations list these members in this order and sizes; a change here must be mirrored in the shaders.
TEST(LightBlockTest, LayoutMatchesTheStd140DeclarationInTheShaders) {
	EXPECT_EQ(0u, offsetof(LightBlock, ambient));
	EXPECT_EQ(16u, offsetof(LightBlock, lightInfo));
	EXPECT_EQ(32u, offsetof(LightBlock, lightVec));
	EXPECT_EQ(96u, offsetof(LightBlock, lightColor));
	EXPECT_EQ(160u, offsetof(LightBlock, lightDir));
	EXPECT_EQ(224u, offsetof(LightBlock, shadowVP));
	EXPECT_EQ(288u, offsetof(LightBlock, shadowInfo));
	EXPECT_EQ(304u, offsetof(LightBlock, areaU));
	EXPECT_EQ(368u, offsetof(LightBlock, areaV));
	EXPECT_EQ(432u, sizeof(LightBlock));
}

TEST(LightBlockTest, DirectionalLightStoresTheDirectionToTheLight) {
	Light sun;
	sun.direction = glm::normalize(glm::vec3(-1.f, -2.f, -1.f));
	sun.color = { 1.f, 0.5f, 0.25f };
	sun.intensity = 2.f;
	LightBlock block;
	ASSERT_EQ(1, packLights(block, { sun }, { 0.1f, 0.2f, 0.3f }));
	EXPECT_EQ(1.f, block.lightInfo.x);
	expectNear(glm::vec3(block.lightVec[0]), glm::normalize(glm::vec3(1.f, 2.f, 1.f)));
	EXPECT_EQ(0.f, block.lightVec[0].w);
	expectNear(glm::vec3(block.lightColor[0]), { 2.f, 1.f, 0.5f });
	EXPECT_LE(block.lightDir[0].w, -1.5f);   // not a spot
	EXPECT_EQ(0.f, block.areaU[0].w);        // not an area light
	expectNear(glm::vec3(block.ambient), { 0.1f, 0.2f, 0.3f });
}

TEST(LightBlockTest, PointLightStoresItsPosition) {
	Light p;
	p.type = LightType::Point;
	p.position = { 1.f, 2.f, 3.f };
	p.intensity = 10.f;
	LightBlock block;
	packLights(block, { p }, glm::vec3(0.f));
	expectNear(glm::vec3(block.lightVec[0]), { 1.f, 2.f, 3.f });
	EXPECT_EQ(1.f, block.lightVec[0].w);
	EXPECT_NEAR(10.f, block.lightColor[0].x, kEps);
}

TEST(LightBlockTest, SpotLightStoresTheConeCosines) {
	Light s;
	s.type = LightType::Spot;
	s.position = { 0.f, 3.f, 0.f };
	s.direction = { 0.f, -1.f, 0.f };
	s.spotAngleRad = glm::radians(30.f);
	s.spotBlend = 0.5f;
	LightBlock block;
	packLights(block, { s }, glm::vec3(0.f));
	expectNear(glm::vec3(block.lightDir[0]), { 0.f, -1.f, 0.f });
	EXPECT_NEAR(std::cos(glm::radians(30.f)), block.lightDir[0].w, kEps);      // outer
	EXPECT_NEAR(std::cos(glm::radians(15.f)), block.lightColor[0].w, kEps);    // inner = angle * (1 - blend)
}

TEST(LightBlockTest, AreaLightStoresItsHalfAxesAndMarksTheSlot) {
	Light a;
	a.type = LightType::Area;
	a.position = { 0.f, 3.f, 0.f };
	a.direction = { 0.f, 0.f, -1.f };   // faces -Z
	a.up = { 0.f, 1.f, 0.f };
	a.areaWidth = 4.f;
	a.areaHeight = 2.f;
	LightBlock block;
	packLights(block, { a }, glm::vec3(0.f));
	EXPECT_EQ(1.f, block.areaU[0].w);
	expectNear(glm::vec3(block.areaU[0]), { 2.f, 0.f, 0.f });   // right = forward x up = +X, half the width
	expectNear(glm::vec3(block.areaV[0]), { 0.f, 1.f, 0.f });
	// The shader recovers the travel direction as cross(V, U).
	expectNear(glm::normalize(glm::cross(glm::vec3(block.areaV[0]), glm::vec3(block.areaU[0]))), { 0.f, 0.f, -1.f });
}

TEST(LightBlockTest, ExtraLightsAreIgnoredAndOldSlotsAreCleared) {
	Light p;
	p.type = LightType::Point;
	LightBlock block;
	EXPECT_EQ(kMaxViewportLights, packLights(block, std::vector<Light>(6, p), glm::vec3(0.f)));
	EXPECT_EQ(static_cast<float>(kMaxViewportLights), block.lightInfo.x);
	Light a;
	a.type = LightType::Area;
	packLights(block, { a, a }, glm::vec3(0.f));
	EXPECT_EQ(1.f, block.areaU[1].w);
	packLights(block, { p }, glm::vec3(0.f));   // a smaller set must not keep the previous area marks
	EXPECT_EQ(0.f, block.areaU[0].w);
	EXPECT_EQ(0.f, block.areaU[1].w);
	EXPECT_EQ(1.f, block.lightInfo.x);
}

TEST(LightBlockTest, AreaBasisStaysFiniteAndFacesTheLightDirection) {
	Light area;
	area.type = LightType::Area;
	area.areaWidth = 4.f;
	area.areaHeight = 2.f;
	// Default, almost parallel, non-orthogonal and missing height axes.
	for (const glm::vec3 up : {glm::vec3(0.f, 1.f, 0.f), glm::vec3(1.0e-7f, 1.f, 0.f),
		glm::vec3(0.f, 1.f, 1.f), glm::vec3(0.f)}) {
		area.up = up;
		LightBlock block;
		packLights(block, {area}, glm::vec3(0.f));
		const glm::vec3 u(block.areaU[0]), v(block.areaV[0]);
		for (int i = 0; i < 3; ++i) {
			EXPECT_TRUE(std::isfinite(u[i]));
			EXPECT_TRUE(std::isfinite(v[i]));
		}
		EXPECT_NEAR(2.f, glm::length(u), kEps);
		EXPECT_NEAR(1.f, glm::length(v), kEps);
		EXPECT_NEAR(0.f, glm::dot(u, v), kEps);
		expectNear(glm::normalize(glm::cross(v, u)), area.direction);
	}
}

TEST(LightBlockTest, DefaultSunMatchesTheFormerFixedLight) {
	const Light sun = defaultSun();
	LightBlock block;
	packLights(block, { sun }, glm::vec3(0.f));
	expectNear(glm::vec3(block.lightVec[0]), glm::normalize(glm::vec3(1.f, 2.f, 1.f)));
	expectNear(glm::vec3(block.lightColor[0]), { 1.f, 1.f, 1.f });
}
