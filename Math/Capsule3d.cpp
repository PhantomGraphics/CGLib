#include "Capsule3d.h"
#include "SafeNormalize.h"
#include "pi.h"

namespace Phantom::Math {

template<typename T>
Capsule3d<T>::Capsule3d() :
	Capsule3d(Vector3d<T>(0,0,0), Vector3d<T>(1,0,0), Vector3d<T>(0,1,0), Vector3d<T>(0,0,1))
{}

template<typename T>
Capsule3d<T>::Capsule3d(const Vector3d<T>& bottom, const Vector3d<T>& uvec, const Vector3d<T>& vvec, const Vector3d<T>& wvec) :
	bottom(bottom),
	uvec(uvec),
	vvec(vvec),
	wvec(wvec)
{}

// Surface is split into three equal-length v-bands: bottom hemisphere [0, 1/3),
// cylindrical side [1/3, 2/3], top hemisphere (2/3, 1]. Each band is continuous
// with its neighbor at the shared equator ring (bottom/top + radial(u)).
template<typename T>
Vector3d<T> Capsule3d<T>::getPosition(const T u, const T v) const
{
	const auto angle = T(2) * u * T(PI);
	const auto radial = std::cos(angle) * uvec + std::sin(angle) * vvec;

	const auto radius = getLength(uvec);
	// Pole direction. A zero-length or non-finite axis degenerates the capsule to a
	// sphere around `bottom`; pick the plane normal (or zero) so results stay finite.
	Vector3d<T> wDir(0, 0, 0);
	if (!tryNormalize(wvec, T(0), wDir)) {
		wDir = Vector3d<T>(0, 0, 0);
		tryNormalizedCross(uvec, vvec, wDir);
	}
	const auto top = bottom + wvec;

	constexpr T third = T(1) / T(3);
	constexpr T halfPi = T(PI) / T(2);

	if (v < third) {
		const auto t = v / third;
		const auto angleFromPole = t * halfPi;
		return bottom - radius * std::cos(angleFromPole) * wDir + std::sin(angleFromPole) * radial;
	}
	else if (v <= T(2) * third) {
		const auto s = (v - third) / third;
		return (bottom + s * wvec) + radial;
	}
	else {
		const auto t = (v - T(2) * third) / third;
		const auto angleFromPole = (T(1) - t) * halfPi;
		return top + radius * std::cos(angleFromPole) * wDir + std::sin(angleFromPole) * radial;
	}
}

template class Capsule3d<float>;
template class Capsule3d<double>;

} // namespace Phantom::Math
