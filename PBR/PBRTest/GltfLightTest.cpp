#include <gtest/gtest.h>
#include "../PBR/GltfLight.h"

#include <cmath>
#include <string>

using namespace Phantom::PBR;

TEST(GltfLightTest, LightEntryCarriesPiTimesTheIntensityAndNoRangeCutoff) {
    Light p;
    p.type = LightType::Point;
    p.position = { 1.f, 2.f, 3.f };
    p.color = { 1.f, 0.5f, 0.25f };
    p.intensity = 10.f;
    const auto e = toLightEntry(p);
    EXPECT_EQ(Phantom::Gltf::LightEntry::Type::Point, e.type);
    EXPECT_NEAR(10.f * kPi, e.intensity, 1.0e-4f);
    EXPECT_LE(e.range, 0.f);   // infinite: inverse-square only, like the viewport
    EXPECT_EQ(glm::vec3(1.f, 2.f, 3.f), e.position);
    EXPECT_EQ(glm::vec3(1.f, 0.5f, 0.25f), e.color);
    EXPECT_FALSE(e.castShadow);
    EXPECT_TRUE(toLightEntry(p, true).castShadow);
}

TEST(GltfLightTest, SpotLightEntryUsesOuterAndInnerHalfAngles) {
    Light s;
    s.type = LightType::Spot;
    s.direction = { 0.f, -1.f, 0.f };
    s.spotAngleRad = glm::radians(30.f);
    s.spotBlend = 0.5f;
    const auto e = toLightEntry(s);
    EXPECT_EQ(Phantom::Gltf::LightEntry::Type::Spot, e.type);
    EXPECT_NEAR(30.f, e.spotAngleDeg, 1.0e-3f);
    EXPECT_NEAR(15.f, e.spotInnerAngleDeg, 1.0e-3f);
    EXPECT_EQ(glm::vec3(0.f, -1.f, 0.f), e.direction);
}

TEST(GltfLightTest, AreaLightBecomesAWideSpotInTheGltfRenderer) {
    Light a;
    a.type = LightType::Area;
    a.direction = { 0.f, -1.f, 0.f };
    const auto e = toLightEntry(a);
    EXPECT_EQ(Phantom::Gltf::LightEntry::Type::Spot, e.type);
    EXPECT_NEAR(kAreaAsSpotAngleDeg, e.spotAngleDeg, 1.0e-4f);
}

TEST(GltfLightTest, PunctualExportUsesLuminousEfficacyAndFlagsAreaLights) {
    Light d;
    d.intensity = 2.f;
    const auto g = toGltfPunctual(d);
    EXPECT_EQ(std::string("Directional"), g.type);
    EXPECT_NEAR(2.f * kGltfLuminousEfficacy, g.intensity, 1.0e-3f);
    EXPECT_FALSE(g.approximated);

    Light s;
    s.type = LightType::Spot;
    s.spotAngleRad = 0.6f;
    s.spotBlend = 0.25f;
    const auto gs = toGltfPunctual(s);
    EXPECT_EQ(std::string("Spot"), gs.type);
    EXPECT_NEAR(0.6f, gs.outerConeAngle, 1.0e-6f);
    EXPECT_NEAR(0.45f, gs.innerConeAngle, 1.0e-6f);

    Light a;
    a.type = LightType::Area;
    const auto ga = toGltfPunctual(a);
    EXPECT_EQ(std::string("Point"), ga.type);   // glTF has no area light
    EXPECT_TRUE(ga.approximated);
}
