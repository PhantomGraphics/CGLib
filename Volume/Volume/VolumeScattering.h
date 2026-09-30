#pragma once

#include "ScalarGrid3D.h"

namespace Phantom {
	namespace Volume {

/**
 * @brief Participating-medium parameters shared by the CPU reference, the GPU
 * raymarcher and the PBVR (docs/todo/SPEC_volume_raymarch.md). The density grid
 * holds a mass density [kg/m^3]; extinction = kExt * density [1/m].
 */
struct ScatteringParams {
	float extinction = 0.05f;                       ///< kExt [m^2/kg]: sigma_t = kExt * rho.
	float albedo = 1.0f;                            ///< Single-scattering albedo.
	float phaseG = 0.6f;                            ///< Henyey-Greenstein asymmetry (>0 = forward).
	Math::Vector3df sunDirection = Math::Vector3df(0.0f, 0.0f, 1.0f);  ///< Unit vector TOWARD the sun.
	float sunIrradiance = 1.0f;                     ///< Sun term scale (radiance units of the output).
	float ambient = 0.0f;                           ///< Constant sky term added to the in-scatter source.
	float stepLength = 0.0f;                        ///< Ray step [same length unit]; 0 = cellSize / 2.
	int maxSteps = 4096;                            ///< Hard cap per ray.
	float minTransmittance = 1.0e-3f;               ///< Early-out threshold.
};

struct RayResult {
	Math::Vector3df radiance = Math::Vector3df(0.0f);  ///< Scattered radiance towards the origin.
	float transmittance = 1.0f;                        ///< Remaining transmittance along the ray.
	int steps = 0;                                     ///< Samples taken (0 if the box was missed).
	bool hitCap = false;                               ///< Stopped by maxSteps while still inside the medium.
};

namespace VolumeScattering {

/** @brief Henyey-Greenstein phase function value for cos(theta) between propagation and scattering directions. */
float henyeyGreenstein(float cosTheta, float g);

/**
 * @brief Ray / box slab test. Returns false if the ray misses or the box lies wholly behind.
 * tNear is clamped to >= 0.
 */
bool intersectBox(const Math::Vector3df& origin, const Math::Vector3df& direction,
                  const Math::Vector3df& boxMin, const Math::Vector3df& boxMax, float& tNear, float& tFar);

/**
 * @brief Transmittance toward the sun for every cell centre: exp(-kExt * integral of density
 * along sunDirection to the box boundary). Border value is 1 (open sky). Computed once per
 * density/sun change and read by every raymarch and PBVR sample.
 */
ScalarGrid3D computeSunTransmittance(const ScalarGrid3D& density, const ScatteringParams& params);

/**
 * @brief Reference single-scattering raymarch. Per step of length ds with midpoint density rho:
 *   sigma = kExt*rho,  a = 1 - exp(-sigma*ds)
 *   L += T * a * albedo * (sunIrradiance * phase(dot(sunDir, dir)) * Tsun(x) + ambient)
 *   T *= exp(-sigma*ds)
 * `direction` must be unit length. `tMax` limits the ray (e.g. an opaque surface depth).
 */
RayResult marchRay(const ScalarGrid3D& density, const ScalarGrid3D& sunTransmittance,
                   const ScatteringParams& params, const Math::Vector3df& origin,
                   const Math::Vector3df& direction, float tMax = 1.0e30f);

/** @brief Effective step length (params.stepLength, or cellSize/2 when 0). */
float effectiveStep(const ScalarGridDesc& desc, const ScatteringParams& params);

}  // namespace VolumeScattering

	}
}
