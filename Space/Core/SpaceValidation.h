#pragma once

#include "CGLib/Math/Core/Vector3d.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Small input checks shared by the grid / hash based searchers.
		 *
		 * Every searcher bins positions with `static_cast<int>(position / cellSize)`. That cast is
		 * undefined for NaN, infinity and out-of-int-range values, and a zero/negative/NaN cell size
		 * turns the division into garbage. These helpers let the searchers reject such input up front
		 * (returning false) instead of reaching the cast. They never throw.
		 *
		 * Cell coordinates follow the existing convention of truncation toward zero (not floor), so the
		 * cell containing the origin is twice as wide. That is harmless for neighbor search (the 27-cell
		 * neighborhood still covers every point within one cell size) and is kept so results do not change.
		 */
		namespace Validation {

			/// Largest |cell coordinate| accepted. Leaves headroom for the +-1 neighbor offsets and for
			/// packing three coordinates into one 64-bit grid id (see IndexedParticle).
			constexpr int kMaxCellCoordinate = 1 << 30;

			/// A cell size / search radius that can be divided by: finite and strictly positive.
			template <typename T>
			inline bool isValidCellSize(const T cellSize)
			{
				return std::isfinite(cellSize) && cellSize > T(0);
			}

			/// A radius for a range query: finite and non-negative (zero is a valid, degenerate radius).
			template <typename T>
			inline bool isValidRadius(const T radius)
			{
				return std::isfinite(radius) && radius >= T(0);
			}

			inline bool isFinite(const Math::Vector3df& v)
			{
				return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
			}

			/**
			 * @brief Converts one coordinate to a cell coordinate (truncation toward zero).
			 *
			 * The division is done in T, exactly like the searchers' own `toIndex()` (float for
			 * IndexedParticle, double for the hashes), so a validated position lands in the same cell it
			 * always did.
			 * @return false if the quotient is not finite or exceeds +-kMaxCellCoordinate.
			 */
			template <typename T>
			inline bool tryToCellCoordinate(const T value, const T cellSize, int& out)
			{
				const T q = value / cellSize;
				if (!std::isfinite(q) || q <= -static_cast<T>(kMaxCellCoordinate) || q >= static_cast<T>(kMaxCellCoordinate)) {
					return false;
				}
				out = static_cast<int>(q);
				return true;
			}

			/// 3D version of tryToCellCoordinate(). `cellSize` must satisfy isValidCellSize().
			template <typename T>
			inline bool tryToCellIndex(const Math::Vector3df& pos, const T cellSize, std::array<int, 3>& out)
			{
				return tryToCellCoordinate(static_cast<T>(pos.x), cellSize, out[0]) &&
					tryToCellCoordinate(static_cast<T>(pos.y), cellSize, out[1]) &&
					tryToCellCoordinate(static_cast<T>(pos.z), cellSize, out[2]);
			}

		}
	}
}
