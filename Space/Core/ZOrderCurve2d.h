#pragma once

#include <array>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Encodes and decodes 2D grid coordinates using the Z-order (Morton) curve.
		 *
		 * Maps a 2D integer coordinate (x, y) to a single unsigned integer by interleaving bits:
		 *   result = (y2 x2 y1 x1 y0 x0 ...)
		 *
		 * Preserves 2D spatial locality in 1D index space.
		 * See ZOrderCurve3d for the 3D equivalent.
		 */
		class ZOrderCurve2d
		{
		public:
			/**
			 * @brief Encodes a 2D unsigned integer index to a Z-order (Morton) value.
			 * @param index Array of two unsigned integers [x, y].
			 * @return The Morton-encoded unsigned integer.
			 */
			unsigned int encode(const std::array<unsigned int, 2>& index) const;

			/**
			 * @brief Decodes a Z-order (Morton) value back to a 2D unsigned integer index.
			 * @param x The Morton-encoded value to decode.
			 * @return Array of two unsigned integers [x, y].
			 */
			std::array<unsigned int, 2> decode(unsigned int x) const;

		private:
			unsigned int encode(unsigned int n) const;

			unsigned int decode_(unsigned int x) const;
		};

	}
}
