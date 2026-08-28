#include "IntersectionCalculator.h"

#include "DistanceCalculator.h"

#include "CGLib/Math/Ray3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Plane3d.h"
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Box3d.h"
#include "CGLib/Math/Rectangle3d.h"
#include "CGLib/Math/Line3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

	template<typename T>
	bool isInside(const Vector3d<T>& p, const Triangle3d<T>& triangle)
	{
		const auto& n = triangle.getNormal();
		const auto& vertices = triangle.getVertices();

		const auto a = vertices[0] - p;
		const auto b = vertices[1] - p;
		const auto c = vertices[2] - p;

		const auto d = vertices[1] - vertices[0];
		const auto e = vertices[2] - vertices[1];
		const auto f = vertices[0] - vertices[2];

		const auto& v1 = glm::cross(a, d);
		if (glm::dot(v1, n) < 0.0) {
			return false;
		}
		const auto& v2 = glm::cross(b, e);
		if (glm::dot(v2, n) < 0.0) {
			return false;
		}
		const auto& v3 = glm::cross(c, f);
		if (glm::dot(v3, n) < 0.0) {
			return false;
		}
		return true;
	}

	template<typename T>
	bool isInside(const Vector3d<T>& p, const Rectangle3d<T>& quad)
	{
		const auto& n = quad.getNormal();
		const auto p0 = quad.getPosition(0,0);
		const auto p1 = quad.getPosition(1, 0);
		const auto p2 = quad.getPosition(0, 1);
		const auto p3 = quad.getPosition(1, 1);

		const auto a = p0 - p;
		const auto b = p1 - p;
		const auto c = p2 - p;
		const auto d = p3 - p;

		const auto aa = p1 - p0;
		const auto bb = p2 - p1;
		const auto cc = p3 - p2;
		const auto dd = p0 - p3;

		const auto& v1 = glm::cross(a, aa);
		if (glm::dot(v1, n) < 0.0) {
			return false;
		}
		const auto& v2 = glm::cross(b, bb);
		if (glm::dot(v2, n) < 0.0) {
			return false;
		}
		const auto& v3 = glm::cross(c, cc);
		if (glm::dot(v3, n) < 0.0) {
			return false;
		}
		const auto& v4 = glm::cross(d, dd);
		if (glm::dot(v4, n) < 0.0) {
			return false;
		}
		return true;
	}
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Ray3d<T>& ray, const Sphere3d<T>& sphere, const T tolerance)
{
	const auto ps = DistanceCalculator<T>::calculate(ray, sphere, tolerance);
	std::vector<T> inners;
	for (auto p : ps) {
		const auto pos = ray.getPosition(p);
		if (sphere.contains(pos, tolerance)) {
			inners.push_back(p);
		}
	}
	return inners;
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Ray3d<T>& ray, const Plane3d<T>& plane, const T tolerance)
{
	const auto distance = plane.getDistance(ray.getOrigin());

	if (distance > -tolerance) {
		return { distance };
	}
	return {};
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Ray3d<T>& ray, const Triangle3d<T>& triangle, const T tolerance)
{
	const auto ps = DistanceCalculator<T>::calculate(ray, triangle, tolerance);
	std::vector<T> inners;
	for (auto p : ps) {
		const auto pos = ray.getPosition(p);
		if (::isInside(pos, triangle)) {
			inners.push_back(p);
		}
	}
	return inners;
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Ray3d<T>& ray, const Rectangle3d<T>& quad, const T tolerance)
{
	const auto& plane = Plane3d<T>(quad.getPosition(0.0, 0.0), quad.getNormal());

	const auto ps = IntersectionCalculator::calculate(ray, plane, tolerance);
	std::vector<T> inners;
	for (auto p : ps) {
		const auto pos = ray.getPosition(p);
		if (::isInside(pos, quad)) {
			inners.push_back(p);
		}
	}
	return inners;
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Ray3d<T>& ray, const Box3d<T>& box, const T tolerance)
{
	const auto ps = DistanceCalculator<T>::calculate(ray, box, tolerance);
	std::vector<T> inners;
	for (auto p : ps) {
		const auto pos = ray.getPosition(p);
		if (box.contains(pos, tolerance)) {
			inners.push_back(p);
		}
	}
	return inners;
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Line3d<T>& line, const Plane3d<T> plane, const T tolerance)
{
	// 線分の始点が三角系の裏側にあれば、当たらない
	const auto planeToStart = plane.getDistance(line.getStart());	// 線分の始点と平面の距離
	if (planeToStart <= tolerance) {
		return {};
	}

	// 線分の終点が三角系の表側にあれば、当たらない
	const auto planeToEnd = plane.getDistance(line.getEnd());	// 線分の終点と平面の距離
	if (planeToEnd >= -tolerance) {
		return {};
	}

	// 直線と平面との交点を取る
	const auto denom = planeToStart - planeToEnd;
	const auto param = planeToStart / denom;

	return { param };
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Line3d<T>& line, const Sphere3d<T>& sphere, const T tolerance)
{
	const auto& dir = glm::normalize(line.getDirection());
	const auto diff = sphere.getCenter() - line.getStart();
	const auto t0 = glm::dot(diff, dir);
	const auto dSquared = dot(diff, diff) - t0 * t0;
	const auto sphereRadiusSquared = sphere.getRadius() * sphere.getRadius();
	if (dSquared > sphereRadiusSquared) {
		return {};
	}
	auto t1 = sqrt(sphereRadiusSquared - dSquared);
	if (t0 < t1 + tolerance) {
		t1 = -t1;
	}
	const auto l = line.getLength();
	const auto p0 = (t0 - t1) / l;
	const auto p1 = (t0 + t1) / l;
	std::vector<T> inners;
	if (0.0 < p0 && p0 <= 1.0) {
		if (sphere.contains(line.getPosition(p0), tolerance)) {
			inners.push_back(p0);
		}
	}
	if (0.0 < p1 && p1 <= 1.0) {
		if (sphere.contains(line.getPosition(p1), tolerance)) {
			inners.push_back(p1);
		}
	}
	return inners;
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Line3d<T>& line, const Triangle3d<T>& triangle, const T tolerance)
{
	const auto& plane = Plane3d<T>(triangle.getVertices()[0], triangle.getNormal());
	const auto ps = IntersectionCalculator::calculate(line, plane, tolerance);
	std::vector<T> inners;
	for (auto p : ps) {
		const auto pos = line.getPosition(p);
		if (::isInside(pos, triangle)) {
			inners.push_back(p);
		}
	}
	return inners;
}

template<typename T>
std::vector<T> IntersectionCalculator<T>::calculate(const Sphere3d<T>& sphere, const Plane3d<T>& plane, const T tolerance)
{
	const auto distance = plane.getDistance(sphere.getCenter());
	if (std::abs(distance) <= sphere.getRadius() + tolerance) {
		return { distance };
	}
	return {};
}


template class Phantom::Space::IntersectionCalculator<float>;
template class Phantom::Space::IntersectionCalculator<double>;