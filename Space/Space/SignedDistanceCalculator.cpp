#include "SignedDistanceCalculator.h"

#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Plane3d.h"
#include "CGLib/Math/Box3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

template<typename T>
T SignedDistanceCalculator<T>::calculate(const Vector3d<T>& position, const Sphere3d<T>& sphere)
{
	const auto d = ::getDistance(position, sphere.getCenter());
	const auto r = sphere.getRadius();
	return d - r;
}

template<typename T>
T SignedDistanceCalculator<T>::calculate(const Vector3d<T>& position, const Plane3d<T>& plane)
{
	const auto v = position - plane.getOrigin();
	const auto d = glm::dot(v, plane.getNormal());
	return d;
}

/*
template<typename T>
T SignedDistanceCalculator<T>::calculate(const Vector3d<T>& position, const Box3d<T>& box)
{
	const auto max = box.getMax();
	const auto minxDist = ::fabs(position.x - box.getMin());
	const auto maxxDist = ::fabs(position.x - box.getMax());
	const auto xdist = minx
}
*/

template class Phantom::Space::SignedDistanceCalculator<float>;
template class Phantom::Space::SignedDistanceCalculator<double>;