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

	Vector3d<T> getNormal(const T u, const T v) const;// const override;

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