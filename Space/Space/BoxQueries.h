#pragma once

#include "CGLib/Math/Box3d.h"
#include "CGLib/Math/Vector3d.h"
#include "SpaceValidation.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Small box tests shared by the tree structures (Octree, BVH).
		 *
		 * Only the per-box arithmetic is shared; the structures keep their own traversal and
		 * subdivision, because their splitting schemes differ and a common base class would hide that.
		 * Each function is the exact computation the structures used inline before, so results are unchanged.
		 */
		namespace BoxQueries {

			/// Squared distance from `p` to the closest point of `box` (0 if `p` is inside or on it).
			inline float squaredDistanceToBox(const Math::Vector3df& p, const Math::Box3df& box)
			{
				const auto bmin = box.getMin();
				const auto bmax = box.getMax();
				// Closest point of the box: clamp the position into it.
				float cx = p.x;
				if (cx < bmin.x) cx = bmin.x;
				if (cx > bmax.x) cx = bmax.x;
				float cy = p.y;
				if (cy < bmin.y) cy = bmin.y;
				if (cy > bmax.y) cy = bmax.y;
				float cz = p.z;
				if (cz < bmin.z) cz = bmin.z;
				if (cz > bmax.z) cz = bmax.z;
				const float dx = p.x - cx;
				const float dy = p.y - cy;
				const float dz = p.z - cz;
				return dx * dx + dy * dy + dz * dz;
			}

			/**
			 * @brief Slab test of the ray `origin + t * dir`, t in [tMin, tMax], against `box`.
			 *
			 * `dir` need not be normalized (t scales with its length). Zero components are tested against
			 * the closed slab directly. Non-finite vectors, zero direction, NaN or reversed t ranges miss.
			 * Infinite range endpoints are allowed.
			 */
			inline bool rayHitsBox(const Math::Box3df& box, const Math::Vector3df& origin, const Math::Vector3df& dir, float tMin, float tMax)
			{
				if (!Validation::isFinite(origin) || !Validation::isFinite(dir) ||
					(dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) ||
					std::isnan(tMin) || std::isnan(tMax) || tMax < tMin) return false;
				float lo = tMin, hi = tMax;
				const auto bmin = box.getMin();
				const auto bmax = box.getMax();
				if (!Validation::isFinite(bmin) || !Validation::isFinite(bmax)) return false;
				for (int i = 0; i < 3; ++i) {
					if (dir[i] == 0.0f) {
						if (origin[i] < bmin[i] || origin[i] > bmax[i]) return false;
						continue;
					}
					const float invD = 1.0f / dir[i];
					float t0 = (bmin[i] - origin[i]) * invD;
					float t1 = (bmax[i] - origin[i]) * invD;
					if (invD < 0.0f) std::swap(t0, t1);
					lo = std::max(lo, t0);
					hi = std::min(hi, t1);
					if (hi < lo) return false;
				}
				return true;
			}

			/// Surface area of `box` (2 * (xy + yz + zx)); the cost measure used by BVH::refitDegradation().
			inline float surfaceArea(const Math::Box3df& box)
			{
				const auto l = box.getLength();
				return 2.0f * (l.x * l.y + l.y * l.z + l.z * l.x);
			}

		}
	}
}
