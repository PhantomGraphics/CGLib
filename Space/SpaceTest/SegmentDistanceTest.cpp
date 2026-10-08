#include <gtest/gtest.h>
#include "CGLib/Space/Space/DistanceCalculator.h"

using namespace Phantom;

TEST(DistanceCalculatorTest, ClosestSegmentsCrossingAndClampedEndpoints) {
    using V = Math::Vector3dd;
    using D = Space::DistanceCalculator<double>;
    auto result = D::closestSegments(V(-1,0,0), V(1,0,0), V(0,-1,0), V(0,1,0));
    EXPECT_NEAR(result.distance, 0., 1.e-12);
    EXPECT_NEAR(result.firstFraction, .5, 1.e-12);
    EXPECT_NEAR(result.secondFraction, .5, 1.e-12);
    EXPECT_NEAR(glm::length(result.normal), 1., 1.e-12);
    result = D::closestSegments(V(2,2,0), V(3,2,0), V(0,-1,0), V(0,1,0));
    EXPECT_NEAR(result.distance, std::sqrt(5.), 1.e-12);
    EXPECT_DOUBLE_EQ(result.firstFraction, 0.);
    EXPECT_DOUBLE_EQ(result.secondFraction, 1.);
}

TEST(DistanceCalculatorTest, ClosestSegmentsParallelDegenerateAndSymmetric) {
    using V = Math::Vector3df;
    using D = Space::DistanceCalculator<float>;
    const auto forward = D::closestSegments(V(0,0,0), V(2,0,0), V(1,1,0), V(3,1,0));
    const auto reverse = D::closestSegments(V(1,1,0), V(3,1,0), V(0,0,0), V(2,0,0));
    EXPECT_NEAR(forward.distance, 1.f, 1.e-6f);
    EXPECT_NEAR(reverse.distance, forward.distance, 1.e-6f);
    EXPECT_NEAR(glm::length(forward.normal+reverse.normal), 0.f, 1.e-6f);
    const auto point = D::closestSegments(V(0,2,0), V(0,2,0), V(0,0,0), V(0,0,0));
    EXPECT_NEAR(point.distance, 2.f, 1.e-6f);
    EXPECT_NEAR(point.normal.y, 1.f, 1.e-6f);
    const auto coincident = D::closestSegments(V(0), V(0), V(0), V(0));
    EXPECT_FLOAT_EQ(coincident.distance, 0.f);
    EXPECT_NEAR(glm::length(coincident.normal), 1.f, 1.e-6f);
}
