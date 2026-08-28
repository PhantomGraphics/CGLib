#pragma once

#include "SparseVolume.h"
#include "Coord.h"

#include <cmath>

namespace Phantom {
namespace Volume {

template<typename T>
class TrilinearInterpolator {
public:
	explicit TrilinearInterpolator(const SparseVolume<T>& sv)
		: sv_(sv) {
	}

	T getValue(const Math::Vector3d<T>& worldPos) const {
		const T voxelSize = static_cast<T>(sv_.getVoxelSize());
		if (voxelSize <= static_cast<T>(0)) {
			return sv_.getBackground();
		}

		const T gx = worldPos.x / voxelSize;
		const T gy = worldPos.y / voxelSize;
		const T gz = worldPos.z / voxelSize;

		const int ix0 = static_cast<int>(std::floor(gx));
		const int iy0 = static_cast<int>(std::floor(gy));
		const int iz0 = static_cast<int>(std::floor(gz));
		const int ix1 = ix0 + 1;
		const int iy1 = iy0 + 1;
		const int iz1 = iz0 + 1;

		const T tx = gx - static_cast<T>(ix0);
		const T ty = gy - static_cast<T>(iy0);
		const T tz = gz - static_cast<T>(iz0);

		const T c000 = sv_.getValue(Coord(ix0, iy0, iz0));
		const T c100 = sv_.getValue(Coord(ix1, iy0, iz0));
		const T c010 = sv_.getValue(Coord(ix0, iy1, iz0));
		const T c110 = sv_.getValue(Coord(ix1, iy1, iz0));
		const T c001 = sv_.getValue(Coord(ix0, iy0, iz1));
		const T c101 = sv_.getValue(Coord(ix1, iy0, iz1));
		const T c011 = sv_.getValue(Coord(ix0, iy1, iz1));
		const T c111 = sv_.getValue(Coord(ix1, iy1, iz1));

		const T c00 = c000 * (static_cast<T>(1) - tx) + c100 * tx;
		const T c10 = c010 * (static_cast<T>(1) - tx) + c110 * tx;
		const T c01 = c001 * (static_cast<T>(1) - tx) + c101 * tx;
		const T c11 = c011 * (static_cast<T>(1) - tx) + c111 * tx;

		const T c0 = c00 * (static_cast<T>(1) - ty) + c10 * ty;
		const T c1 = c01 * (static_cast<T>(1) - ty) + c11 * ty;

		return c0 * (static_cast<T>(1) - tz) + c1 * tz;
	}

	Math::Vector3d<T> getGradient(const Math::Vector3d<T>& worldPos, float h = 1.0f) const {
		const T step = static_cast<T>(h) * static_cast<T>(sv_.getVoxelSize());
		if (step <= static_cast<T>(0)) {
			return Math::Vector3d<T>(0, 0, 0);
		}

		const Math::Vector3d<T> dx(step, 0, 0);
		const Math::Vector3d<T> dy(0, step, 0);
		const Math::Vector3d<T> dz(0, 0, step);

		const T gx = (getValue(worldPos + dx) - getValue(worldPos - dx)) / (static_cast<T>(2) * step);
		const T gy = (getValue(worldPos + dy) - getValue(worldPos - dy)) / (static_cast<T>(2) * step);
		const T gz = (getValue(worldPos + dz) - getValue(worldPos - dz)) / (static_cast<T>(2) * step);

		return Math::Vector3d<T>(gx, gy, gz);
	}

private:
	const SparseVolume<T>& sv_;
};

} // namespace Math
} // namespace Phantom
