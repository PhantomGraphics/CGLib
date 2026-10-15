// Phase 5: result contracts of the geometric queries for degenerate input (docs/spec/space_api_contracts.md §12).
//
// The rule: a query returns finite values or an explicit "no result" (an empty vector); it never returns
// NaN/Inf or a bogus hit for non-finite input, a zero-length direction, a zero-length segment or a
// zero-area triangle/rectangle. Behavior for ordinary input is unchanged (the existing calculator tests
// still pin it).
#include "gtest/gtest.h"

#include "../Space/DistanceCalculator.h"
#include "../Space/Intersection2d.h"
#include "../Space/IntersectionCalculator.h"
#include "../Space/SignedDistanceCalculator.h"

#include "CGLib/Math/Math/Box3d.h"
#include "CGLib/Math/Math/Circle2d.h"
#include "CGLib/Math/Math/Line2d.h"
#include "CGLib/Math/Math/Line3d.h"
#include "CGLib/Math/Math/Plane3d.h"
#include "CGLib/Math/Math/Ray3d.h"
#include "CGLib/Math/Math/Rectangle3d.h"
#include "CGLib/Math/Math/Sphere3d.h"
#include "CGLib/Math/Math/Triangle3d.h"

#include <cmath>
#include <limits>
#include <random>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
	constexpr float kInf = std::numeric_limits<float>::infinity();
	constexpr float tol = 1.0e-6f;

	bool allFinite(const std::vector<float>& v)
	{
		for (const float x : v) if (!std::isfinite(x)) return false;
		return true;
	}

	Triangle3df triangle(const Vector3df& a, const Vector3df& b, const Vector3df& c)
	{
		return Triangle3df({ a, b, c });
	}
	const Triangle3df& unitTriangle()
	{
		static const Triangle3df t = triangle(Vector3df(0, 0, 0), Vector3df(10, 0, 0), Vector3df(0, 10, 0));
		return t;
	}
}

// ---------------------------------------------------------------- ordinary input is unchanged

TEST(GeometryContractTest, RaySphereTangentReportsTheDoubleRoot)
{
	Ray3df ray(Vector3df(0, 1, 0), Vector3df(1, 0, 0)); // grazes the unit sphere at (10, 1, 0)
	Sphere3df sphere(Vector3df(10, 0, 0), 1.0f);
	const auto t = DistanceCalculator<float>::calculate(ray, sphere, tol);
	ASSERT_EQ(t.size(), 2u);
	EXPECT_FLOAT_EQ(t[0], t[1]);
	EXPECT_FLOAT_EQ(t[0], 10.0f);
}

TEST(GeometryContractTest, RaySphereBehindTheOriginIsStillReported)
{
	// The sphere/box overloads solve for the infinite line: negative t (behind the origin) is returned.
	Ray3df ray(Vector3df(0, 0, 0), Vector3df(1, 0, 0));
	Sphere3df sphere(Vector3df(-10, 0, 0), 1.0f);
	const auto t = DistanceCalculator<float>::calculate(ray, sphere, tol);
	ASSERT_EQ(t.size(), 2u);
	EXPECT_FLOAT_EQ(t[0], -11.0f);
	EXPECT_FLOAT_EQ(t[1], -9.0f);
}

TEST(GeometryContractTest, RayBoxFromInsideReportsEntryBehindAndExitAhead)
{
	Ray3df ray(Vector3df(5, 5, 5), Vector3df(1, 0.3f, 0.2f));
	Box3df box(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	const auto t = DistanceCalculator<float>::calculate(ray, box, tol);
	ASSERT_EQ(t.size(), 2u);
	EXPECT_LT(t[0], 0.0f);
	EXPECT_GT(t[1], 0.0f);
}

TEST(GeometryContractTest, RayTriangleHitsEdgesAndVerticesAndIgnoresBehind)
{
	const auto& tri = unitTriangle();
	// Straight down onto the interior, an edge (u == 0) and a vertex.
	for (const Vector3df& target : { Vector3df(2, 2, 0), Vector3df(0, 5, 0), Vector3df(0, 0, 0), Vector3df(5, 0, 0) }) {
		Ray3df ray(target + Vector3df(0, 0, 3), Vector3df(0, 0, -1));
		const auto t = DistanceCalculator<float>::calculate(ray, tri, tol);
		ASSERT_EQ(t.size(), 1u) << target.x << "," << target.y;
		EXPECT_FLOAT_EQ(t[0], 3.0f);
	}
	// Triangle behind the ray origin -> no hit (this overload is a half-line, unlike sphere/box).
	Ray3df away(Vector3df(2, 2, 3), Vector3df(0, 0, 1));
	EXPECT_TRUE(DistanceCalculator<float>::calculate(away, tri, tol).empty());
	// Parallel ray -> no hit.
	Ray3df parallel(Vector3df(2, 2, 3), Vector3df(1, 0, 0));
	EXPECT_TRUE(DistanceCalculator<float>::calculate(parallel, tri, tol).empty());
}

TEST(GeometryContractTest, RayTriangleParameterScalesWithDirectionLength)
{
	const auto& tri = unitTriangle();
	Ray3df unit(Vector3df(2, 2, 6), Vector3df(0, 0, -1));
	Ray3df doubled(Vector3df(2, 2, 6), Vector3df(0, 0, -2));
	const auto tu = DistanceCalculator<float>::calculate(unit, tri, tol);
	const auto td = DistanceCalculator<float>::calculate(doubled, tri, tol);
	ASSERT_EQ(tu.size(), 1u);
	ASSERT_EQ(td.size(), 1u);
	EXPECT_FLOAT_EQ(tu[0], 6.0f);
	EXPECT_FLOAT_EQ(td[0], 3.0f) << "t counts direction lengths, so doubling the direction halves t";
}

// ---------------------------------------------------------------- non-unit direction

TEST(GeometryContractTest, RaySphereWithNonUnitDirectionMeasuresTInDirectionLengths)
{
	Sphere3df sphere(Vector3df(10, 0, 0), 1.0f);
	for (const float length : { 0.25f, 2.0f, 7.5f }) {
		Ray3df ray(Vector3df(0, 0, 0), Vector3df(length, 0, 0));
		const auto t = DistanceCalculator<float>::calculate(ray, sphere, tol);
		ASSERT_EQ(t.size(), 2u) << length;
		EXPECT_NEAR(t[0] * length, 9.0f, 1.0e-4f) << length;
		EXPECT_NEAR(t[1] * length, 11.0f, 1.0e-4f) << length;
		// The returned parameters reach the surface.
		for (const float tt : t) {
			const auto p = ray.getPosition(tt);
			EXPECT_NEAR(glm::length(Vector3dd(p) - Vector3dd(10, 0, 0)), 1.0, 1.0e-4);
		}
	}
}

TEST(GeometryContractTest, RayBoxWithNonUnitDirectionMeasuresTInDirectionLengths)
{
	Box3df box(Vector3df(4, -1, -1), Vector3df(6, 1, 1));
	Ray3df ray(Vector3df(0, 0, 0), Vector3df(2, 0.25f, 0)); // x in [4,6] at t in [2,3]; y stays inside until t=4
	const auto t = DistanceCalculator<float>::calculate(ray, box, tol);
	ASSERT_EQ(t.size(), 2u);
	EXPECT_NEAR(t[0], 2.0f, 1.0e-5f);
	EXPECT_NEAR(t[1], 3.0f, 1.0e-5f);
}

// ---------------------------------------------------------------- zero direction / axis-parallel

TEST(GeometryContractTest, ZeroDirectionRayFindsNothing)
{
	const Vector3df zero(0, 0, 0);
	Ray3df ray(Vector3df(0, 0, 0), zero);
	EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, Sphere3df(Vector3df(0, 0, 0), 1.0f), tol).empty());
	EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, Sphere3df(Vector3df(5, 0, 0), 1.0f), tol).empty());
	EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, unitTriangle(), tol).empty());
	EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, Box3df(Vector3df(-1, -1, -1), Vector3df(1, 1, 1)), tol).empty());
	EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, Box3df(Vector3df(5, 5, 5), Vector3df(6, 6, 6)), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, Sphere3df(Vector3df(0, 0, 0), 1.0f), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, Box3df(Vector3df(-1, -1, -1), Vector3df(1, 1, 1)), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, unitTriangle(), tol).empty());
}

TEST(GeometryContractTest, AxisParallelRayAgainstBoxUsesTheSlabs)
{
	Box3df box(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	// Parallel to X, inside the Y/Z slabs: hits with finite entry/exit.
	{
		Ray3df ray(Vector3df(-5, 5, 5), Vector3df(1, 0, 0));
		const auto t = DistanceCalculator<float>::calculate(ray, box, tol);
		ASSERT_EQ(t.size(), 2u);
		EXPECT_FLOAT_EQ(t[0], 5.0f);
		EXPECT_FLOAT_EQ(t[1], 15.0f);
	}
	// Parallel to X but outside the Y slab: miss.
	{
		Ray3df ray(Vector3df(-5, 11, 5), Vector3df(1, 0, 0));
		EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, box, tol).empty());
	}
	// Exactly on a slab plane (0 * inf would be NaN): the closed box includes its boundary.
	{
		Ray3df ray(Vector3df(-5, 0, 5), Vector3df(1, 0, 0));
		const auto t = DistanceCalculator<float>::calculate(ray, box, tol);
		ASSERT_EQ(t.size(), 2u);
		EXPECT_TRUE(allFinite(t));
		EXPECT_FLOAT_EQ(t[0], 5.0f);
		EXPECT_FLOAT_EQ(t[1], 15.0f);
	}
	// Two zero components.
	{
		Ray3df ray(Vector3df(5, 5, -3), Vector3df(0, 0, 1));
		const auto t = DistanceCalculator<float>::calculate(ray, box, tol);
		ASSERT_EQ(t.size(), 2u);
		EXPECT_FLOAT_EQ(t[0], 3.0f);
		EXPECT_FLOAT_EQ(t[1], 13.0f);
	}
	// The intersection overload reports only parameters that land on the box.
	{
		Ray3df ray(Vector3df(-5, 5, 5), Vector3df(1, 0, 0));
		const auto t = IntersectionCalculator<float>::calculate(ray, box, tol);
		EXPECT_TRUE(allFinite(t));
		EXPECT_EQ(t.size(), 2u);
	}
}

// ---------------------------------------------------------------- non-finite input

TEST(GeometryContractTest, NonFiniteInputNeverProducesNonFiniteResults)
{
	const std::vector<Vector3df> bad = { Vector3df(kNaN, 0, 0), Vector3df(0, kInf, 0), Vector3df(0, 0, -kInf) };
	const Sphere3df sphere(Vector3df(5, 0, 0), 1.0f);
	const Box3df box(Vector3df(4, -1, -1), Vector3df(6, 1, 1));
	for (const auto& b : bad) {
		const Ray3df badOrigin(b, Vector3df(1, 0, 0));
		const Ray3df badDirection(Vector3df(0, 0, 0), b);
		for (const Ray3df& ray : { badOrigin, badDirection }) {
			EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, sphere, tol).empty());
			EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, box, tol).empty());
			EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, unitTriangle(), tol).empty());
			EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, sphere, tol).empty());
			EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, box, tol).empty());
			EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, unitTriangle(), tol).empty());
			EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, Plane3df(Vector3df(0, 0, 0), Vector3df(0, 0, 1)), tol).empty());
		}
		const Line3df badLine(Vector3df(0, 0, 1), b);
		EXPECT_TRUE(IntersectionCalculator<float>::calculate(badLine, sphere, tol).empty());
		EXPECT_TRUE(IntersectionCalculator<float>::calculate(badLine, unitTriangle(), tol).empty());
		EXPECT_TRUE(IntersectionCalculator<float>::calculate(badLine, Plane3df(Vector3df(0, 0, 0), Vector3df(0, 0, 1)), tol).empty());
	}
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(Sphere3df(Vector3df(kNaN, 0, 0), 1.0f), Plane3df(Vector3df(0, 0, 0), Vector3df(0, 0, 1)), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(Ray3df(Vector3df(0, 0, 0), Vector3df(1, 0, 0)), Sphere3df(Vector3df(5, 0, 0), kNaN), tol).empty());
}

// ---------------------------------------------------------------- zero-length segments and degenerate faces

TEST(GeometryContractTest, ZeroLengthLineFindsNothing)
{
	const Line3df point(Vector3df(5, 0, 0), Vector3df(5, 0, 0));
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(point, Sphere3df(Vector3df(5, 0, 0), 1.0f), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(point, Sphere3df(Vector3df(50, 0, 0), 1.0f), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(point, unitTriangle(), tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(point, Plane3df(Vector3df(0, 0, 0), Vector3df(1, 0, 0)), tol).empty());
}

TEST(GeometryContractTest, DegenerateTrianglesAreNeverHit)
{
	const std::vector<Triangle3df> degenerate = {
		triangle(Vector3df(0, 0, 0), Vector3df(5, 0, 0), Vector3df(10, 0, 0)),   // collinear
		triangle(Vector3df(1, 1, 1), Vector3df(1, 1, 1), Vector3df(1, 1, 1)),    // a point
		triangle(Vector3df(0, 0, 0), Vector3df(0, 0, 0), Vector3df(4, 4, 4)),    // repeated vertex
	};
	for (const auto& tri : degenerate) {
		// A ray and a segment that pass right through the triangle's position.
		const Ray3df ray(Vector3df(1, 1, 5), Vector3df(0, 0, -1));
		const Line3df line(Vector3df(1, 1, 5), Vector3df(1, 1, -5));
		const Ray3df rayThroughPoint(Vector3df(2.5f, 0, 5), Vector3df(0, 0, -1));
		EXPECT_TRUE(DistanceCalculator<float>::calculate(ray, tri, tol).empty());
		EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, tri, tol).empty());
		EXPECT_TRUE(IntersectionCalculator<float>::calculate(line, tri, tol).empty()) << "was NaN (normal of a zero-area face)";
		EXPECT_TRUE(IntersectionCalculator<float>::calculate(rayThroughPoint, tri, tol).empty());
	}
}

TEST(GeometryContractTest, DegenerateRectangleIsNeverHit)
{
	const Ray3df ray(Vector3df(0, 0, 5), Vector3df(0, 0, -1));
	// Zero-length u vector, and parallel u/v vectors (zero area).
	const Rectangle3df noWidth(Vector3df(0, 0, 0), Vector3df(0, 0, 0), Vector3df(0, 4, 0));
	const Rectangle3df parallel(Vector3df(0, 0, 0), Vector3df(2, 0, 0), Vector3df(4, 0, 0));
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, noWidth, tol).empty());
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(ray, parallel, tol).empty());
}

TEST(GeometryContractTest, PointToTriangleDistanceIsFiniteForDegenerateTriangles)
{
	// Closest point of a collinear triangle is the closest point of the segment hull.
	const auto line = triangle(Vector3df(0, 0, 0), Vector3df(5, 0, 0), Vector3df(10, 0, 0));
	EXPECT_NEAR((DistanceCalculator<float>::calculate(line, Vector3df(3, 4, 0), 0.0f)), 4.0f, 1.0e-5f);
	EXPECT_NEAR((DistanceCalculator<float>::calculate(line, Vector3df(-3, 4, 0), 0.0f)), 5.0f, 1.0e-5f);
	EXPECT_NEAR((DistanceCalculator<float>::calculate(line, Vector3df(13, 4, 0), 0.0f)), 5.0f, 1.0e-5f);

	const auto point = triangle(Vector3df(1, 1, 1), Vector3df(1, 1, 1), Vector3df(1, 1, 1));
	EXPECT_NEAR((DistanceCalculator<float>::calculate(point, Vector3df(1, 1, 4), 0.0f)), 3.0f, 1.0e-5f);

	// Query points over every region of a collinear triangle used to hit the 1/0 face-region formula.
	std::mt19937 rng(5u);
	std::uniform_real_distribution<float> d(-15.0f, 15.0f);
	for (int i = 0; i < 500; ++i) {
		const Vector3df p(d(rng), d(rng), d(rng));
		const float dist = DistanceCalculator<float>::calculate(line, p, 0.0f);
		EXPECT_TRUE(std::isfinite(dist)) << p.x << "," << p.y << "," << p.z;
		EXPECT_GE(dist, 0.0f);
	}
}

TEST(GeometryContractTest, PointToTriangleDistanceOrdinaryCasesAreUnchanged)
{
	const auto& tri = unitTriangle();
	EXPECT_NEAR((DistanceCalculator<float>::calculate(tri, Vector3df(2, 2, 3), 0.0f)), 3.0f, 1.0e-5f);   // face
	EXPECT_NEAR((DistanceCalculator<float>::calculate(tri, Vector3df(5, -2, 0), 0.0f)), 2.0f, 1.0e-5f);  // edge AB
	EXPECT_NEAR((DistanceCalculator<float>::calculate(tri, Vector3df(-3, -4, 0), 0.0f)), 5.0f, 1.0e-5f); // vertex A
	EXPECT_FLOAT_EQ((DistanceCalculator<float>::calculate(tri, Vector3df(2, 2, 1.0e-7f), 1.0e-6f)), 0.0f) << "within tolerance -> 0";
}

TEST(GeometryContractTest, RepeatedTriangleVerticesUseTheClosedSegmentDistance)
{
	// Exercise every vertex ordering in float and double, including projection beyond an endpoint.
	for (int distinct = 0; distinct < 3; ++distinct) {
		std::array<Vector3df, 3> vertices = { Vector3df(0), Vector3df(0), Vector3df(0) };
		vertices[distinct] = Vector3df(1, 0, 0);
		const Triangle3df tf(vertices);
		const Triangle3d<double> td({ Vector3dd(vertices[0]), Vector3dd(vertices[1]), Vector3dd(vertices[2]) });
		for (const float x : { -1.0f, 0.0f, 0.5f, 1.0f, 2.0f }) {
			const float dx = x < 0 ? -x : (x > 1 ? x - 1 : 0);
			const double expected = std::sqrt(1.0 + double(dx) * dx);
			EXPECT_NEAR(DistanceCalculator<float>::calculate(tf, Vector3df(x, 1, 0), 0.0f), expected, 1.0e-6);
			EXPECT_NEAR(DistanceCalculator<double>::calculate(td, Vector3dd(x, 1, 0), 0.0), expected, 1.0e-12);
		}
		EXPECT_FLOAT_EQ(DistanceCalculator<float>::calculate(tf, Vector3df(0.5f, 1.0e-7f, 0), 1.0e-6f), 0.0f);
	}
}

TEST(GeometryContractTest, ZeroDirectionLineAgainstCircleFindsNothing)
{
	const Line2df point(Vector2df(1, 0), Vector2df(1, 0));
	const Circle2df circle(1.0f, Vector2df(0, 0));
	const auto found = Intersection2d<float>::calculate(point, circle, tol);
	EXPECT_TRUE(found.empty()) << "a zero-length segment has no parameter to report (was 0/0)";
	// An ordinary secant still gives its two parameters.
	const Line2df secant(Vector2df(-2, 0), Vector2df(2, 0));
	const auto two = Intersection2d<float>::calculate(secant, circle, tol);
	ASSERT_EQ(two.size(), 2u);
	EXPECT_NEAR(two[0], 0.25f, 1.0e-6f);
	EXPECT_NEAR(two[1], 0.75f, 1.0e-6f);
}

// ---------------------------------------------------------------- planes and signed distance

TEST(GeometryContractTest, SignedDistanceSignConvention)
{
	// Outside a sphere is positive, inside negative; the front of a plane (along its normal) is positive.
	const Sphere3df sphere(Vector3df(0, 0, 0), 2.0f);
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(5, 0, 0), sphere)), 3.0f);
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(1, 0, 0), sphere)), -1.0f);
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(2, 0, 0), sphere)), 0.0f);
	const Plane3df plane(Vector3df(0, 1, 0), Vector3df(0, 1, 0));
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(0, 4, 0), plane)), 3.0f);
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(0, -2, 0), plane)), -3.0f);
}

TEST(GeometryContractTest, PlaneDistanceIsScaledByTheNormalLength)
{
	// Plane normals are NOT normalized: a plane built with a normal of length 2 reports distances twice
	// as large. Callers must pass a unit normal when they want a metric distance.
	const Plane3df unit(Vector3df(0, 0, 0), Vector3df(0, 1, 0));
	const Plane3df doubled(Vector3df(0, 0, 0), Vector3df(0, 2, 0));
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(0, 3, 0), unit)), 3.0f);
	EXPECT_FLOAT_EQ((SignedDistanceCalculator<float>::calculate(Vector3df(0, 3, 0), doubled)), 6.0f);
	EXPECT_FLOAT_EQ(unit.getDistance(Vector3df(0, 3, 0)), SignedDistanceCalculator<float>::calculate(Vector3df(0, 3, 0), unit))
		<< "the two entry points agree";
}

TEST(GeometryContractTest, RayPlaneReportsTheOriginsSignedDistanceNotARayParameter)
{
	// Documented quirk kept for compatibility: the Ray/Plane overload ignores the direction and returns the
	// origin's signed distance when the origin is in front of (or on) the plane.
	const Plane3df plane(Vector3df(0, 0, 0), Vector3df(0, 1, 0));
	const auto front = IntersectionCalculator<float>::calculate(Ray3df(Vector3df(0, 3, 0), Vector3df(1, 0, 0)), plane, tol);
	ASSERT_EQ(front.size(), 1u);
	EXPECT_FLOAT_EQ(front[0], 3.0f);
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(Ray3df(Vector3df(0, -3, 0), Vector3df(0, 1, 0)), plane, tol).empty())
		<< "behind the plane: nothing, even though this ray points at it";
}

TEST(GeometryContractTest, SegmentCrossingAPlaneIsOneSidedFrontToBack)
{
	const Plane3df plane(Vector3df(0, 0, 0), Vector3df(0, 1, 0));
	const auto down = IntersectionCalculator<float>::calculate(Line3df(Vector3df(0, 2, 0), Vector3df(0, -2, 0)), plane, tol);
	ASSERT_EQ(down.size(), 1u);
	EXPECT_FLOAT_EQ(down[0], 0.5f);
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(Line3df(Vector3df(0, -2, 0), Vector3df(0, 2, 0)), plane, tol).empty())
		<< "back-to-front crossings are ignored";
	EXPECT_TRUE(IntersectionCalculator<float>::calculate(Line3df(Vector3df(0, 0, 0), Vector3df(0, -2, 0)), plane, tol).empty())
		<< "a segment starting on the plane is not a crossing";
}

// ---------------------------------------------------------------- float / double agreement

TEST(GeometryContractTest, FloatAndDoubleAgreeOnOrdinaryInput)
{
	std::mt19937 rng(11u);
	std::uniform_real_distribution<double> pos(-5.0, 5.0), dirc(-1.0, 1.0);
	int compared = 0;
	for (int i = 0; i < 300; ++i) {
		const Vector3dd o(pos(rng), pos(rng), pos(rng));
		Vector3dd d(dirc(rng), dirc(rng), dirc(rng));
		if (glm::length(d) < 0.2) continue;
		d = glm::normalize(d);
		const Vector3dd c(pos(rng), pos(rng), pos(rng));
		const double r = 0.5 + std::abs(pos(rng)) * 0.3;

		const auto td = DistanceCalculator<double>::calculate(Ray3dd(o, d), Sphere3dd(c, r), 1.0e-9);
		const auto tf = DistanceCalculator<float>::calculate(Ray3df(Vector3df(o), Vector3df(d)), Sphere3df(Vector3df(c), static_cast<float>(r)), 1.0e-6f);
		ASSERT_EQ(td.size(), tf.size()) << i;
		for (size_t k = 0; k < td.size(); ++k) EXPECT_NEAR(td[k], tf[k], 2.0e-3) << i;

		const Box3dd boxD(c - Vector3dd(r), c + Vector3dd(r));
		const Box3df boxF(Vector3df(c - Vector3dd(r)), Vector3df(c + Vector3dd(r)));
		const auto bd = DistanceCalculator<double>::calculate(Ray3dd(o, d), boxD, 1.0e-9);
		const auto bf = DistanceCalculator<float>::calculate(Ray3df(Vector3df(o), Vector3df(d)), boxF, 1.0e-6f);
		ASSERT_EQ(bd.size(), bf.size()) << i;
		for (size_t k = 0; k < bd.size(); ++k) EXPECT_NEAR(bd[k], bf[k], 2.0e-3) << i;
		compared += static_cast<int>(td.size() + bd.size());
	}
	EXPECT_GT(compared, 50) << "the random scene must actually produce hits";
}
