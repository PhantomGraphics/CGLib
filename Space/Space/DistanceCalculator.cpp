#include "DistanceCalculator.h"

#include "CGLib/Math/Ray3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Box3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

// 点と三角形の距離（最近接点を求めるアルゴリズム：Ericson）
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
	const auto& direction = ray.getDirection();
	const auto& diff = sphere.getCenter() - ray.getOrigin();
	const auto t0 = glm::dot(diff, direction);
	const auto distanceSquared = glm::dot(diff, diff) - t0 * t0;
	const auto sphereRadiusSquared = sphere.getRadius() * sphere.getRadius();
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
	// reference https://pheema.hatenablog.jp/entry/ray-triangle-intersection
	const auto v0 = triangle.getVertices()[0];
	const auto v1 = triangle.getVertices()[1];
	const auto v2 = triangle.getVertices()[2];

	const auto e1 = v1 - v0;
	const auto e2 = v2 - v0;

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
	// tmin, tmax -> parameter
	const auto origin = ray.getOrigin();
	const auto dir = ray.getDirection();

	const auto min = box.getMin();
	const auto max = box.getMax();
	auto tmin = (min.x - origin.x) / dir.x;
	auto tmax = (max.x - origin.x) / dir.x;

	if (tmin > tmax) std::swap(tmin, tmax);

	auto tymin = (min.y - origin.y) / dir.y;
	auto tymax = (max.y - origin.y) / dir.y;

	if (tymin > tymax) std::swap(tymin, tymax);

	if ((tmin > tymax) || (tymin > tmax))
		return {};

	if (tymin > tmin)
		tmin = tymin;

	if (tymax < tmax)
		tmax = tymax;

	auto tzmin = (min.z - origin.z) / dir.z;
	auto tzmax = (max.z - origin.z) / dir.z;

	if (tzmin > tzmax) std::swap(tzmin, tzmax);

	if ((tmin > tzmax) || (tzmin > tmax))
		return {};

	if (tzmin > tmin)
		tmin = tzmin;

	if (tzmax < tmax)
		tmax = tzmax;

	return { tmin, tmax };
}


template class Phantom::Space::DistanceCalculator<float>;
template class Phantom::Space::DistanceCalculator<double>;