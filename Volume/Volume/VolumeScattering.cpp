#include "VolumeScattering.h"

#include <algorithm>
#include <cmath>

namespace Phantom {
	namespace Volume {
		namespace VolumeScattering {

namespace {
constexpr float kPi = 3.14159265358979323846f;
using Math::Vector3df;

float dot3(const Vector3df& a, const Vector3df& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
}

float henyeyGreenstein(float cosTheta, float g)
{
	const float denom = 1.0f + g * g - 2.0f * g * cosTheta;
	return (1.0f - g * g) / (4.0f * kPi * std::pow(std::max(denom, 1.0e-6f), 1.5f));
}

bool intersectBox(const Vector3df& o, const Vector3df& d, const Vector3df& bmin, const Vector3df& bmax,
                  float& tNear, float& tFar)
{
	float t0 = 0.0f;
	float t1 = 1.0e30f;
	for (int a = 0; a < 3; ++a) {
		if (std::abs(d[a]) < 1.0e-12f) {
			if (o[a] < bmin[a] || o[a] > bmax[a]) return false;
			continue;
		}
		float ta = (bmin[a] - o[a]) / d[a];
		float tb = (bmax[a] - o[a]) / d[a];
		if (ta > tb) std::swap(ta, tb);
		t0 = std::max(t0, ta);
		t1 = std::min(t1, tb);
		if (t0 > t1) return false;
	}
	tNear = t0;
	tFar = t1;
	return true;
}

float effectiveStep(const ScalarGridDesc& desc, const ScatteringParams& params)
{
	return params.stepLength > 0.0f ? params.stepLength : 0.5f * desc.cellSize;
}

ScalarGrid3D computeSunTransmittance(const ScalarGrid3D& density, const ScatteringParams& params)
{
	const ScalarGridDesc& desc = density.desc();
	ScalarGrid3D out(desc, 1.0f, 1.0f);
	const float ds = effectiveStep(desc, params);
	const Vector3df bmin = desc.origin;
	const Vector3df bmax = desc.maxCorner();
	for (uint32_t k = 0; k < desc.nz; ++k) {
		for (uint32_t j = 0; j < desc.ny; ++j) {
			for (uint32_t i = 0; i < desc.nx; ++i) {
				const Vector3df c = density.cellCenter(i, j, k);
				float tn = 0.0f, tf = 0.0f;
				if (!intersectBox(c, params.sunDirection, bmin, bmax, tn, tf)) continue;
				// Optical depth by midpoint rule from the cell centre to the box exit.
				float tau = 0.0f;
				const int n = std::max(1, static_cast<int>(std::ceil(tf / ds)));
				const float h = tf / static_cast<float>(n);
				for (int s = 0; s < n; ++s) {
					tau += density.sample(c + params.sunDirection * ((s + 0.5f) * h)) * h;
				}
				out.at(i, j, k) = std::exp(-params.extinction * tau);
			}
		}
	}
	return out;
}

RayResult marchRay(const ScalarGrid3D& density, const ScalarGrid3D& sunT, const ScatteringParams& params,
                   const Vector3df& origin, const Vector3df& direction, float tMax)
{
	RayResult r;
	const ScalarGridDesc& desc = density.desc();
	float tn = 0.0f, tf = 0.0f;
	if (!intersectBox(origin, direction, desc.origin, desc.maxCorner(), tn, tf)) return r;
	tf = std::min(tf, tMax);
	if (tf <= tn) return r;

	const float ds0 = effectiveStep(desc, params);
	const float span = tf - tn;
	const int wanted = std::max(1, static_cast<int>(std::ceil(span / ds0)));
	const int n = std::min(wanted, params.maxSteps);
	const float ds = span / static_cast<float>(wanted);   // keep the exact ds even when capped
	const float phase = henyeyGreenstein(dot3(params.sunDirection, direction), params.phaseG);

	float T = 1.0f;
	Vector3df L(0.0f);
	for (int s = 0; s < n; ++s) {
		const float t = tn + (s + 0.5f) * ds;
		const Vector3df p = origin + direction * t;
		const float sigma = params.extinction * density.sample(p);
		++r.steps;
		if (sigma <= 0.0f) continue;
		const float stepT = std::exp(-sigma * ds);
		const float src = params.albedo * (params.sunIrradiance * phase * sunT.sample(p) + params.ambient);
		L += Vector3df(T * (1.0f - stepT) * src);
		T *= stepT;
		if (T < params.minTransmittance) break;
	}
	if (n < wanted && T >= params.minTransmittance) r.hitCap = true;
	r.radiance = L;
	r.transmittance = T;
	return r;
}

		}
	}
}
