#pragma once

#include "CGLib/Math/Vector3d.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Phantom {
	namespace Volume {

/**
 * @brief Geometry of a dense, axis-aligned scalar grid. Values live at cell
 * centres: sample (i, j, k) sits at origin + (i + 0.5, j + 0.5, k + 0.5) * cellSize.
 * Units are the caller's (the cloud renderer uses metres).
 */
struct ScalarGridDesc {
	uint32_t nx = 0, ny = 0, nz = 0;
	Math::Vector3df origin = Math::Vector3df(0.0f);  ///< Minimum corner of the box.
	float cellSize = 1.0f;

	size_t cellCount() const { return static_cast<size_t>(nx) * ny * nz; }
	Math::Vector3df extent() const { return Math::Vector3df(nx, ny, nz) * cellSize; }
	Math::Vector3df maxCorner() const { return origin + extent(); }
	bool operator==(const ScalarGridDesc& o) const
	{
		return nx == o.nx && ny == o.ny && nz == o.nz && origin == o.origin && cellSize == o.cellSize;
	}
};

/**
 * @brief Dense scalar field with trilinear sampling; `borderValue` is what lies
 * outside the box (0 for density, 1 for a transmittance grid). This is the CPU
 * reference of the GPU 3D image that the raymarcher and the PBVR both read.
 */
class ScalarGrid3D {
public:
	ScalarGrid3D() = default;
	explicit ScalarGrid3D(const ScalarGridDesc& d, float fill = 0.0f, float border = 0.0f)
		: desc_(d), data_(d.cellCount(), fill), border_(border) {}

	const ScalarGridDesc& desc() const { return desc_; }
	float borderValue() const { return border_; }
	std::vector<float>& data() { return data_; }
	const std::vector<float>& data() const { return data_; }

	size_t index(uint32_t i, uint32_t j, uint32_t k) const
	{
		return (static_cast<size_t>(k) * desc_.ny + j) * desc_.nx + i;
	}
	float& at(uint32_t i, uint32_t j, uint32_t k) { return data_[index(i, j, k)]; }
	float at(uint32_t i, uint32_t j, uint32_t k) const { return data_[index(i, j, k)]; }

	Math::Vector3df cellCenter(uint32_t i, uint32_t j, uint32_t k) const
	{
		return desc_.origin + (Math::Vector3df(i, j, k) + 0.5f) * desc_.cellSize;
	}

	/** @brief Trilinear interpolation; cells outside the grid read as borderValue. */
	float sample(const Math::Vector3df& p) const
	{
		const Math::Vector3df g = (p - desc_.origin) / desc_.cellSize - 0.5f;
		const float fx = std::floor(g.x), fy = std::floor(g.y), fz = std::floor(g.z);
		const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy), z0 = static_cast<int>(fz);
		const float tx = g.x - fx, ty = g.y - fy, tz = g.z - fz;
		auto v = [&](int x, int y, int z) {
			if (x < 0 || y < 0 || z < 0 || x >= static_cast<int>(desc_.nx) ||
			    y >= static_cast<int>(desc_.ny) || z >= static_cast<int>(desc_.nz)) {
				return border_;
			}
			return data_[index(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z))];
		};
		const float c00 = v(x0, y0, z0) * (1 - tx) + v(x0 + 1, y0, z0) * tx;
		const float c10 = v(x0, y0 + 1, z0) * (1 - tx) + v(x0 + 1, y0 + 1, z0) * tx;
		const float c01 = v(x0, y0, z0 + 1) * (1 - tx) + v(x0 + 1, y0, z0 + 1) * tx;
		const float c11 = v(x0, y0 + 1, z0 + 1) * (1 - tx) + v(x0 + 1, y0 + 1, z0 + 1) * tx;
		return (c00 * (1 - ty) + c10 * ty) * (1 - tz) + (c01 * (1 - ty) + c11 * ty) * tz;
	}

private:
	ScalarGridDesc desc_;
	std::vector<float> data_;
	float border_ = 0.0f;
};

	}
}
