#pragma once

#include <array>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Encodes and decodes 3D grid coordinates using the Z-order (Morton) curve.
		 *
		 * The Z-order curve maps a 3D integer coordinate (x, y, z) to a single unsigned integer
		 * by interleaving the bits of each coordinate:
		 *   result = (z2 y2 x2 z1 y1 x1 z0 y0 x0 ...)
		 *
		 * This encoding preserves spatial locality: nearby 3D coordinates tend to map to
		 * nearby 1D values, which improves cache performance in spatial data structures.
		 *
		 * All methods are static.
		 */
		class ZOrderCurve3d
		{
		public:
			/**
			 * @brief Encodes a 3D unsigned integer index to a Z-order (Morton) value.
			 *
			 * Each component's bits are spread apart and interleaved with the others.
			 *
			 * @param index Array of three unsigned integers [x, y, z].
			 * @return The Morton-encoded unsigned integer.
			 */
			static unsigned int encode(const std::array<unsigned int, 3>& index);

			/**
			 * @brief Decodes a Z-order (Morton) value back to a 3D unsigned integer index.
			 * @param x The Morton-encoded value to decode.
			 * @return Array of three unsigned integers [x, y, z].
			 */
			static std::array<unsigned int, 3> decode(const unsigned int x);

			/**
			 * @brief Computes the Z-order index of the smallest common ancestor
			 *        of the two given Z-order cells.
			 *
			 * Used to navigate implicit Z-order tree structures (e.g., linear octrees).
			 *
			 * @param ltd Z-order index of the left-top-deep corner cell.
			 * @param rbd Z-order index of the right-bottom-front corner cell.
			 * @return Z-order index of the parent (common ancestor) node.
			 */
			static unsigned int getParent(unsigned int ltd, unsigned int rbd);

		private:
			static unsigned int encode(unsigned int x);

			static unsigned int decode_(unsigned int x);
		};

	}
}
