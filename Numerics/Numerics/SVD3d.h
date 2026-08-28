#pragma once

#include "CGLib/Math/Vector3d.h"
#include "CGLib/Math/Matrix3d.h"

namespace Phantom {
	namespace Numerics {

/**
 * @brief Eigenvalue / singular-value decomposition for 3x3 matrices.
 *
 * Provides two decomposition methods with different underlying algorithms
 * and different assumptions about the input matrix:
 *
 * | Method            | Algorithm                       | Input assumption | Value ordering |
 * |-------------------|---------------------------------|-----------------|----------------|
 * | `calculate()`     | Eigen SelfAdjointEigenSolver    | Symmetric only  | Ascending      |
 * | `calculateJacobi()` | Eigen JacobiSVD (full U)      | Any real matrix | Descending     |
 *
 * ## Result fields reuse
 * Both methods populate the same `Result` struct.  When using `calculateJacobi()`,
 * `Result::eigenValues` holds **singular values** and `Result::eigenVectors`
 * holds the **left singular matrix U** 窶・not eigenvectors in the mathematical sense.
 *
 * @see SVD2d for the 2x2 symmetric-only equivalent.
 */
class SVD3d
{
public:
	/**
	 * @brief Result of the decomposition.
	 *
	 * Field semantics depend on the method called:
	 * - `calculate()`      竊・eigenvalues (ascending) / eigenvectors (columns of V).
	 * - `calculateJacobi()` 竊・singular values (descending) / left singular matrix U.
	 */
	struct Result
	{
		bool isOk;                    ///< @c true if the solver converged.
		Math::Vector3dd eigenValues;  ///< Eigenvalues or singular values (see method docs).
		Math::Matrix3dd eigenVectors; ///< Eigenvector matrix or U matrix (see method docs).
	};

	/**
	 * @brief Eigenvalue decomposition for a 3x3 **symmetric** matrix.
	 *
	 * Uses `Eigen::SelfAdjointEigenSolver`.  Eigenvalues are returned in
	 * **ascending order**; columns of `Result::eigenVectors` are the
	 * corresponding unit eigenvectors.
	 *
	 * @param lhs Symmetric input matrix (upper/lower triangle must match).
	 * @return    Decomposition result.  Check @c Result::isOk before use.
	 */
	Result calculate(const Math::Matrix3dd& lhs);

	/**
	 * @brief Singular value decomposition (SVD) for a 3x3 matrix via the Jacobi algorithm.
	 *
	 * Uses `Eigen::JacobiSVD` with the `ComputeFullU` flag.
	 * - `Result::eigenValues`  窶・singular values in **descending order**.
	 * - `Result::eigenVectors` 窶・left singular matrix **U** (orthogonal).
	 *
	 * Unlike `calculate()`, this method works on **any** real 3x3 matrix,
	 * not just symmetric ones.  The right singular matrix V is not computed.
	 *
	 * @param lhs Input matrix (need not be symmetric).
	 * @return    SVD result.  `isOk` is always set to @c true by this method.
	 */
	Result calculateJacobi(const Math::Matrix3dd& lhs);

	/**
	 * @brief Result of a full SVD (both singular matrices) for a 3x3 matrix.
	 */
	struct FullResult
	{
		bool isOk;                    ///< @c true if the solver converged.
		Math::Vector3dd singularValues; ///< Singular values in descending order.
		Math::Matrix3dd matrixU;       ///< Left singular matrix U (orthogonal).
		Math::Matrix3dd matrixV;       ///< Right singular matrix V (orthogonal).
	};

	/**
	 * @brief Full singular value decomposition (both U and V) for a 3x3 matrix via the
	 * Jacobi algorithm.
	 *
	 * Uses `Eigen::JacobiSVD` with `ComputeFullU | ComputeFullV`, so that
	 * `lhs == matrixU * diag(singularValues) * matrixV.transpose()`. Needed for
	 * rotation-fitting problems (e.g. the Kabsch algorithm used by ICPRegistration)
	 * where both singular matrices are required, unlike `calculateJacobi()` which
	 * only computes U.
	 *
	 * @param lhs Input matrix (need not be symmetric).
	 * @return    Full SVD result. `isOk` is always set to @c true by this method.
	 */
	FullResult calculateFullJacobi(const Math::Matrix3dd& lhs);
};

	}
}