#include "DistanceCalculator.h"
#include "GeometryGuards.h"
#include <algorithm>
#include <cmath>
#include <limits>

#include "CGLib/Math/Ray3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Box3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

// Closest points on two closed segments (Ericson).
template<typename T>
SegmentDistanceResult<T> DistanceCalculator<T>::closestSegments(const Vector3d<T>& p,
    const Vector3d<T>& q, const Vector3d<T>& secondStart, const Vector3d<T>& secondEnd) {
    const auto d1 = q-p;
    const auto d2 = secondEnd-secondStart;
    const auto r = p-secondStart;
    const T a = glm::dot(d1, d1), e = glm::dot(d2, d2);
    const T b = glm::dot(d1, d2), f = glm::dot(d2, r);
    T s = T(0.), t = T(0.);
    if (a <= T(1.e-12)) {
        if (e > T(1.e-12)) t = std::clamp(f/e, T(0.), T(1.));
    } else {
        const T cr = glm::dot(d1, r);
        if (e <= T(1.e-12)) s = std::clamp(-cr/a, T(0.), T(1.));
        else {
            const T denominator = a*e-b*b;
            if (denominator > T(1.e-6)*a*e) s = std::clamp((b*f-cr*e)/denominator, T(0.), T(1.));
            t = (b*s+f)/e;
            if (t < T(0.)) { t = T(0.); s = std::clamp(-cr/a, T(0.), T(1.)); }
            else if (t > T(1.)) { t = T(1.); s = std::clamp((b-cr)/a, T(0.), T(1.)); }
        }
    }
    const auto diff = p+s*d1 - (secondStart+t*d2);
    const T distance = glm::length(diff);
    SegmentDistanceResult<T> out;
    out.firstFraction = s; out.secondFraction = t;
    out.distance = distance;
    if (distance > T(1.e-7)) out.normal = diff/distance;
    else {
        // A deterministic perpendicular also resolves a particle at a sphere centre.
        const auto axis = e > T(1.e-12) ? d2 : d1;
        if (glm::length(axis) > T(1.e-7)) {
            const auto unit = glm::normalize(axis);
            const Math::Vector3d<T> basis = std::abs(unit.x) < T(0.8) ? Math::Vector3d<T>(T(1.),T(0.),T(0.)) : Math::Vector3d<T>(T(0.),T(0.),T(1.));
            out.normal = glm::normalize(glm::cross(unit, basis));
        }
    }
    return out;
}

template<typename T>
T DistanceCalculator<T>::calculate(const Triangle3d<T>& triangle, const Vector3d<T>& point, const T tolerance)
{
	const auto verts = triangle.getVertices();
	const auto A = verts[0];
	const auto B = verts[1];
	const auto C = verts[2];

	const auto AB = B - A;
	const auto AC = C - A;
	const auto AP = point - A;

	const T d1 = glm::dot(AB, AP);
	const T d2 = glm::dot(AC, AP);
	// vertex region A
	if (d1 <= T(0) && d2 <= T(0)) {
		const T dist = glm::length(point - A);
		return dist <= tolerance ? T(0) : dist;
	}

	const auto BP = point - B;
	const T d3 = glm::dot(AB, BP);
	const T d4 = glm::dot(AC, BP);
	// vertex region B
	if (d3 >= T(0) && d4 <= d3) {
		const T dist = glm::length(point - B);
		return dist <= tolerance ? T(0) : dist;
	}

	// edge region AB
	const T vc = d1 * d4 - d3 * d2;
	if (vc <= T(0) && d1 >= T(0) && d3 <= T(0)) {
		const T v = d1 / (d1 - d3);
		const auto closest = A + AB * v;
		const T dist = glm::length(point - closest);
		return dist <= tolerance ? T(0) : dist;
	}

	const auto CP = point - C;
	const T d5 = glm::dot(AB, CP);
	const T d6 = glm::dot(AC, CP);
	// vertex region C
	if (d6 >= T(0) && d5 <= d6) {
		const T dist = glm::length(point - C);
		return dist <= tolerance ? T(0) : dist;
	}

	// edge region AC
	const T vb = d5 * d2 - d1 * d6;
	if (vb <= T(0) && d2 >= T(0) && d6 <= T(0)) {
		const T w = d2 / (d2 - d6);
		const auto closest = A + AC * w;
		const T dist = glm::length(point - closest);
		return dist <= tolerance ? T(0) : dist;
	}

	// edge region BC
	const T va = d3 * d6 - d5 * d4;
	if (va <= T(0) && (d4 - d3) >= T(0) && (d5 - d6) >= T(0)) {
		const T w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		const auto closest = B + (C - B) * w;
		const T dist = glm::length(point - closest);
		return dist <= tolerance ? T(0) : dist;
	}

	// face region (内部)
	const T denom = T(1) / (va + vb + vc);
	const T v = vb * denom;
	const T w = vc * denom;
	const auto closest = A + AB * v + AC * w;
	const T dist = glm::length(point - closest);
	return dist <= tolerance ? T(0) : dist;
	return T(0);
}

template<typename T>
std::vector<T> DistanceCalculator<T>::calculate(const Ray3d<T>& ray, const Sphere3d<T>& sphere, const T tolerance)
{
	if (!GeometryGuards::isUsable(ray) || !GeometryGuards::isUsable(sphere))
	{
		return {};
	}
	const auto& direction = ray.getDirection();
	const auto& diff = sphere.getCenter() - ray.getOrigin();
	const auto sphereRadiusSquared = sphere.getRadius() * sphere.getRadius();

	// |direction|^2. For a unit direction (the common case, up to normalization rounding) the original
	// formulas below are used unchanged; otherwise the quadratic is solved with the real length so that
	// t is measured in direction lengths, like the triangle and box overloads.
	const auto a = glm::dot(direction, direction);
	if (std::abs(a - T(1)) > T(1.0e-5))
	{
		const auto t0 = glm::dot(diff, direction) / a;
		const auto distanceSquared = glm::dot(diff, diff) - t0 * t0 * a;
		if (distanceSquared > sphereRadiusSquared)
		{
			return {};
		}
		const auto t1 = std::sqrt((sphereRadiusSquared - distanceSquared) / a);
		return { t0 - t1, t0 + t1 };
	}

	const auto t0 = glm::dot(diff, direction);
	const auto distanceSquared = glm::dot(diff, diff) - t0 * t0;
	if (distanceSquared > sphereRadiusSquared)
	{
		return {};
	}
	const auto t1 = std::sqrt(sphereRadiusSquared - distanceSquared);
	return { t0 - t1, t0 + t1 };
}

template<typename T>
std::vector<T> DistanceCalculator<T>::calculate(const Ray3d<T>& ray, const Triangle3d<T>& triangle, const T tolerance)
{
	if (!GeometryGuards::isUsable(ray))
	{
		return {};
	}
	// reference https://pheema.hatenablog.jp/entry/ray-triangle-intersection
	const auto v0 = triangle.getVertices()[0];
	const auto v1 = triangle.getVertices()[1];
	const auto v2 = triangle.getVertices()[2];

	const auto e1 = v1 - v0;
	const auto e2 = v2 - v0;
	if (!GeometryGuards::isFinite(e1) || !GeometryGuards::isFinite(e2))
	{
		return {};
	}

	const auto o = ray.getOrigin();
	const auto d = ray.getDirection();
	const auto alpha = glm::cross(d, e2);
	const auto det = glm::dot(e1, alpha);

	// 三角形に対して、レイが平行に入射するような場合 det = 0 となる。
	// det が小さすぎると 1/det が大きくなりすぎて数値的に不安定になるので
	// det ≈ 0 の場合は交差しないこととする。
	if (-tolerance < det && det < tolerance) {
		return {};
	}

	const auto invDet = 1.0 / det;
	const auto r = o - v0;

	// u が 0 <= u <= 1 を満たしているかを調べる。
	const auto u = glm::dot(alpha, r) * invDet;
	if (u < 0.0 || u > 1.0) {
		return {};
	}

	const auto beta = glm::cross(r, e1);

	// v が 0 <= v <= 1 かつ u + v <= 1 を満たすことを調べる。
	// すなわち、v が 0 <= v <= 1 - u をみたしているかを調べればOK。
	const auto v = glm::dot(d, beta) * invDet;
	if (v < 0.0f || u + v > 1.0f) {
		return {};
	}

	// t が 0 <= t を満たすことを調べる。
	const auto t = glm::dot(e2, beta) * invDet;
	if (t < 0.0f) {
		return {};
	}

	const auto param = Vector3d<T>(t, u, v);
	return { param.x };
}

// ref https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
template<typename T>
std::vector<T> DistanceCalculator<T>::calculate(const Ray3d<T>& ray, const Box3d<T>& box, const T tolerance)
{
	if (!GeometryGuards::isUsable(ray) || !GeometryGuards::isFinite(box.getMin()) || !GeometryGuards::isFinite(box.getMax()))
	{
		return {};
	}

	// Slab test, one axis at a time: the ray is inside the slab for t in [t0, t1]. A direction component
	// of exactly 0 makes the slab either always or never satisfied (dividing would give +-inf, or NaN
	// when the origin lies exactly on a slab plane): outside the slab -> miss, inside (planes included,
	// the box is closed) -> the axis does not constrain t.
	const auto origin = ray.getOrigin();
	const auto dir = ray.getDirection();
	const auto min = box.getMin();
	const auto max = box.getMax();

	constexpr T inf = std::numeric_limits<T>::infinity();
	T tmin = -inf;
	T tmax = inf;
	for (int axis = 0; axis < 3; ++axis)
	{
		T t0;
		T t1;
		if (dir[axis] == T(0))
		{
			if (origin[axis] < min[axis] || origin[axis] > max[axis])
			{
				return {};
			}
			continue;
		}
		t0 = (min[axis] - origin[axis]) / dir[axis];
		t1 = (max[axis] - origin[axis]) / dir[axis];
		if (t0 > t1) std::swap(t0, t1);

		if (t0 > tmax || tmin > t1)
			return {};
		if (t0 > tmin)
			tmin = t0;
		if (t1 < tmax)
			tmax = t1;
	}

	return { tmin, tmax };
}


template class Phantom::Space::DistanceCalculator<float>;
template class Phantom::Space::DistanceCalculator<double>;
