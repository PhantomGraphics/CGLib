
#pragma once

#include "CGLib/Math/Vector2d.h"
#include "CGLib/Math/Matrix2d.h"

namespace Phantom {
	namespace Numerics {

/**
 * @brief Eigenvalue decomposition for 2x2 symmetric (self-adjoint) matrices.
 *
 * Despite the "SVD" name, this class wraps Eigen's `SelfAdjointEigenSolver`,
 * which computes the **eigenvalue decomposition** A = V * D * V^T of a real
 * symmetric matrix 窶・not a general singular value decomposition.
 *
 * ## When to use
 * Use this class when the input matrix is guaranteed to be symmetric
 * (e.g. covariance matrices, inertia tensors, stress tensors).
 * For non-symmetric matrices, use `SVD3d::calculateJacobi()` instead.
 *
 * ## Output ordering
 * Eigenvalues are returned in **ascending order** (Eigen convention).
 * The corresponding eigenvectors are the columns of `Result::eigenVectors`.
 *
 * ## Example
 * @code
 *   Math::Matrix2dd M(1.0, 2.0, 2.0, 3.0);
 *   SVD2d svd;
 *   auto r = svd.calculate(M);
 *   // r.eigenValues[0] ~= -0.2361  (smallest)
 *   // r.eigenValues[1] ~=  4.2361  (largest)
 * @endcode
 */
class SVD2d
{
public:
	/**
	 * @brief Result of the eigenvalue decomposition.
	 */
	struct Result
	{
		bool isOk;                    ///< @c true if the solver converged.
		Math::Vector2dd eigenValues;  ///< Eigenvalues sorted in ascending order.
		Math::Matrix2dd eigenVectors; ///< Columns are the corresponding unit eigenvectors.
	};

	/**
	 * @brief Compute the eigenvalue decomposition of a 2x2 symmetric matrix.
	 *
	 * Internally converts @p lhs to an Eigen::Matrix2d and runs
	 * `Eigen::SelfAdjointEigenSolver`.
	 *
	 * @param lhs Input matrix.  Must be symmetric; behaviour is undefined for
	 *            non-symmetric inputs.
	 * @return    Result containing eigenvalues and eigenvectors.
	 *            Check @c Result::isOk before using the values.
	 */
	Result calculate(const Math::Matrix2dd& lhs);
};

	}
}