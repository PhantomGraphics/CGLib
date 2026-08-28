#include "Triangle3d.h"

using namespace Phantom::Math;

template<typename T>
Triangle3d<T>::Triangle3d() :
	Triangle3d({ Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0) })
{}

template<typename T>
Triangle3d<T>::Triangle3d(const std::array<Vector3d<T>, 3>& vertices) :
	vertices(vertices)
{}

template<typename T>
Vector3d<T> Triangle3d<T>::getNormal() const
{
	const auto v1 = vertices[1] - vertices[0];
	const auto v2 = vertices[2] - vertices[0];
	return glm::normalize(glm::cross(v1, v2));
}

template<typename T>
T Triangle3d<T>::getArea() const
{
	const auto v1 = vertices[1] - vertices[0];
	const auto v2 = vertices[2] - vertices[0];
	return glm::length(glm::cross(v1, v2)) * T(0.5);
}

template<typename T>
bool Triangle3d<T>::isSame(const Triangle3d<T>& rhs, const T tolerance) const
{
	return
		::areSame(vertices[0], rhs.vertices[0], tolerance) &&
		::areSame(vertices[1], rhs.vertices[1], tolerance) &&
		::areSame(vertices[2], rhs.vertices[2], tolerance);
}

template class Phantom::Math::Triangle3d<float>;
template class Phantom::Math::Triangle3d<double>;