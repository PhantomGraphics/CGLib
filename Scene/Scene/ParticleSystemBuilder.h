#pragma once

#include <vector>
#include <memory>

#include "CGLib/Math/Vector3d.h"

#include "ParticleSystem.h"

namespace Phantom {
	namespace Math {
		template<typename T>
		class ICurve3d;
		template<typename T>
		class ISurface3d;
		template<typename T>
		class IVolume3d;
	}
	namespace Shape {

/// @brief Builder that samples geometric primitives into a ParticleSystem.
class ParticleSystemBuilder
{
public:
	/// @brief Sample points along a 3D curve and add them as particles.
	/// @param curve The curve to sample.
	/// @param unum  Number of sample points along the curve.
	void add(const Math::ICurve3d<float>& curve, int unum);

	/// @brief Sample points on a 3D surface and add them as particles.
	/// @param surface The surface to sample.
	/// @param unum    Number of sample points in the u direction.
	/// @param vnum    Number of sample points in the v direction.
	void add(const Math::ISurface3d<float>& surface, const int unum, const int vnum);

	/// @brief Sample points inside a 3D volume and add them as particles.
	/// @param volume The volume to sample.
	/// @param unum   Number of sample points in the u direction.
	/// @param vnun   Number of sample points in the v direction.
	/// @param wnum   Number of sample points in the w direction.
	void add(const Math::IVolume3d<float>& volume, const int unum, const int vnun, const int wnum);

	/// @brief Build and return the resulting ParticleSystem.
	/// @return Owning pointer to the constructed ParticleSystem.
	std::unique_ptr<ParticleSystem> toParticleSystem();

private:
	std::vector<Math::Vector3df> positions;
};

	}
}
