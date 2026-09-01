#pragma once

#include "CGLib/Math/Matrix2d.h"
#include "CGLib/Math/Matrix3d.h"
#include "CGLib/Math/Matrix4d.h"

#include "CGLib/Math/Vector2d.h"
#include "CGLib/Math/Vector3d.h"
#include "CGLib/Math/Vector4d.h"

#include "../ThirdParty/eigen-3.4.0/Eigen/Eigen"


namespace Phantom {
	namespace Numerics {

/**
 * @brief Bidirectional converter between Phantom math types and Eigen types.
 *
 * Phantom uses its own Math::Matrix/Vector classes (suffix `dd` = double precision),
 * while Eigen is used internally for numerical algorithms.  This utility class
 * provides element-wise conversion in both directions so that the rest of the
 * Numerics library can call Eigen solvers without exposing Eigen in public APIs.
 *
 * All methods are static; the class is not intended to be instantiated.
 *
 * ## Index convention
 * Phantom matrices use `[row][col]` (operator[]) while Eigen uses `(row, col)`.
 * Conversion copies each element individually, preserving row-major semantics.
 *
 * ## Supported sizes
 * | Phantom type     | Eigen type        |
 * |------------------|-------------------|
 * | Math::Matrix2dd  | Eigen::Matrix2d   |
 * | Math::Matrix3dd  | Eigen::Matrix3d   |
 * | Math::Matrix4dd  | Eigen::Matrix4d   |
 * | Math::Vector2dd  | Eigen::Vector2d   |
 * | Math::Vector3dd  | Eigen::Vector3d   |
 * | Math::Vector4dd  | Eigen::Vector4d   |
 */
class Converter
{
public:
	/** @brief Convert a 2x2 double-precision Phantom matrix to an Eigen matrix. */
	static Eigen::Matrix2d toEigen(const Math::Matrix2dd& src);

	/** @brief Convert a 3x3 double-precision Phantom matrix to an Eigen matrix. */
	static Eigen::Matrix3d toEigen(const Math::Matrix3dd& src);

	/** @brief Convert a 4x4 double-precision Phantom matrix to an Eigen matrix. */
	static Eigen::Matrix4d toEigen(const Math::Matrix4dd& src);

	/** @brief Convert a 2-component Phantom vector to an Eigen column vector. */
	static Eigen::Vector2d toEigen(const Math::Vector2dd& src);

	/** @brief Convert a 3-component Phantom vector to an Eigen column vector. */
	static Eigen::Vector3d toEigen(const Math::Vector3dd& src);

	/** @brief Convert a 4-component Phantom vector to an Eigen column vector. */
	static Eigen::Vector4d toEigen(const Math::Vector4dd& src);

	/** @brief Convert a 2x2 Eigen matrix to a Phantom matrix. */
	static Math::Matrix2dd fromEigen(const Eigen::Matrix2d& src);

	/** @brief Convert a 3x3 Eigen matrix to a Phantom matrix. */
	static Math::Matrix3dd fromEigen(const Eigen::Matrix3d& src);

	/** @brief Convert a 4x4 Eigen matrix to a Phantom matrix. */
	static Math::Matrix4dd fromEigen(const Eigen::Matrix4d& src);

	/** @brief Convert a 2-component Eigen vector to a Phantom vector. */
	static Math::Vector2dd fromEigen(const Eigen::Vector2d& src);

	/** @brief Convert a 3-component Eigen vector to a Phantom vector. */
	static Math::Vector3dd fromEigen(const Eigen::Vector3d& src);

	/** @brief Convert a 4-component Eigen vector to a Phantom vector. */
	static Math::Vector4dd fromEigen(const Eigen::Vector4d& src);
};
	}
}