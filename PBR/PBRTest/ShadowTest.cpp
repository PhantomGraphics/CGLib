#include <gtest/gtest.h>
#include "../PBR/Shadow.h"

#include <cmath>

using namespace Phantom::PBR;

namespace {
	glm::vec3 project(const glm::mat4& vp, const glm::vec3& p) {
		const glm::vec4 c = vp * glm::vec4(p, 1.f);
		return glm::vec3(c) / c.w;
	}
}

TEST(ShadowTest, OnlyDirectionalAndSpotLightsThatAskForItCast) {
	Light sun, spot, point, area, quiet;
	spot.type = LightType::Spot;
	point.type = LightType::Point;
	area.type = LightType::Area;
	quiet.castShadow = false;
	EXPECT_TRUE(canCastShadow(sun));
	EXPECT_TRUE(canCastShadow(spot));
	EXPECT_FALSE(canCastShadow(point));
	EXPECT_FALSE(canCastShadow(area));
	EXPECT_FALSE(canCastShadow(quiet));
	EXPECT_EQ(-1, pickShadowCaster({}));
	EXPECT_EQ(-1, pickShadowCaster({ point, area, quiet }));
	EXPECT_EQ(2, pickShadowCaster({ point, quiet, spot, sun }));   // the first one that can
}

TEST(ShadowTest, DirectionalMapCoversTheCastersInsideTheClipVolume) {
	Light sun;
	sun.direction = glm::normalize(glm::vec3(-1.f, -2.f, -1.f));
	ShadowSetup s;
	ASSERT_TRUE(computeShadowSetup(sun, { -1.f, 0.f, -1.f }, { 1.f, 2.f, 1.f }, 2048.f, s));
	for (int c = 0; c < 8; ++c) {
		const glm::vec3 corner((c & 1) ? 1.f : -1.f, (c & 2) ? 2.f : 0.f, (c & 4) ? 1.f : -1.f);
		const glm::vec3 n = project(s.viewProj, corner);
		EXPECT_LE(std::abs(n.x), 1.0001f);
		EXPECT_LE(std::abs(n.y), 1.0001f);
		EXPECT_GE(n.z, 0.f);
		EXPECT_LE(n.z, 1.f);
	}
	const float radius = 0.5f * glm::length(glm::vec3(2.f, 2.f, 2.f));
	EXPECT_NEAR(2.f * radius / 2048.f, s.texelBase, 1.0e-6f);
	EXPECT_EQ(0.f, s.texelPerDistance);
}

TEST(ShadowTest, SpotMapLooksAlongTheConeAndTexelSizeGrowsWithDistance) {
	Light spot;
	spot.type = LightType::Spot;
	spot.position = { 0.f, 5.f, 0.f };
	spot.direction = { 0.f, -1.f, 0.f };
	spot.spotAngleRad = glm::radians(40.f);
	ShadowSetup s;
	ASSERT_TRUE(computeShadowSetup(spot, { -1.f, 0.f, -1.f }, { 1.f, 1.f, 1.f }, 2048.f, s));
	const glm::vec3 n = project(s.viewProj, { 0.f, 0.5f, 0.f });   // straight below the light: the centre of the map
	EXPECT_NEAR(0.f, n.x, 1.0e-4f);
	EXPECT_NEAR(0.f, n.y, 1.0e-4f);
	EXPECT_GT(n.z, 0.f);
	EXPECT_LT(n.z, 1.f);
	const float fov = 2.f * glm::radians(40.f) + 0.1f;
	EXPECT_NEAR(2.f * std::tan(fov * 0.5f) / 2048.f, s.texelPerDistance, 1.0e-7f);
	EXPECT_EQ(0.f, s.texelBase);
}

TEST(ShadowTest, ACastlessLightLeavesTheOutputAndTheBlockCarriesTheSetup) {
	Light point;
	point.type = LightType::Point;
	ShadowSetup s;
	s.depthBias = 0.123f;
	EXPECT_FALSE(computeShadowSetup(point, glm::vec3(-1.f), glm::vec3(1.f), 2048.f, s));
	EXPECT_EQ(0.123f, s.depthBias);

	ShadowSetup set;
	set.texelBase = 0.01f;
	set.texelPerDistance = 0.02f;
	set.viewProj = glm::mat4(2.f);
	LightBlock block;
	setBlockShadow(block, 3, set);
	EXPECT_EQ(3.f, block.shadowInfo.x);
	EXPECT_EQ(0.01f, block.shadowInfo.y);
	EXPECT_EQ(set.depthBias, block.shadowInfo.z);
	EXPECT_EQ(0.02f, block.shadowInfo.w);
	EXPECT_EQ(2.f, block.shadowVP[0][0]);
}
