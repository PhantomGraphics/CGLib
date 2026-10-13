#pragma once
#pragma once

#include "Vector3d.h"
#include "Box3d.h"
#include "ISurface3d.h"
#include "IVolume3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Sphere3d : public ISurface3d<T>, public IVolume3d<T>
{
public:
	Sphere3d();

	Sphere3d(const Vector3d<T>& center, const T radius);

	Vector3d<T> getPosition(const T u, const T v) const override;

	Vector3d<T> getPosition(const T u, const T v, const T w) const override;

	/// Unit normal normalize(position - center) at (u, v) (a negative radius therefore
	/// gives the inward direction). Returns the zero vector when the sphere is degenerate
	/// (radius 0 or non-finite); use tryGetNormal() to tell the cases apart.
	Vector3d<T> getNormal(const T u, const T v) const;// const override;

	/// Writes the unit normal to `out` and returns true; returns false and leaves `out`
	/// unchanged for a zero-radius or non-finite sphere.
	bool tryGetNormal(const T u, const T v, Vector3d<T>& out) const;

	Vector3d<T> getCenter() const { return center; }

	T getRadius() const { return radius; }

	void setRadius(const T r) { this->radius = r; }

	Box3d<T> getBoundingBox() const;

	bool contains(const Vector3d<T>& p, const T tolerance) const;

private:
	Vector3d<T> center;
	T radius;
};

using Sphere3df = Sphere3d<float>;
using Sphere3dd = Sphere3d<double>;

	}
}