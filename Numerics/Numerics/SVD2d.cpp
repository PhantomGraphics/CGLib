#include "pch.h"

#include "SVD2d.h"

#include "Converter.h"

#include <iostream>

using namespace Phantom::Math;
using namespace Phantom::Numerics;

SVD2d::Result SVD2d::calculate(const Matrix2dd& lhs)
{
	SVD2d::Result result;
	Eigen::Matrix2d A = Converter::toEigen(lhs);
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eigensolver(A);
	if (eigensolver.info() != Eigen::Success) {
		result.isOk = false;
	}
	result.isOk = true;
	result.eigenValues = Converter::fromEigen(eigensolver.eigenvalues());
	result.eigenVectors = Converter::fromEigen(eigensolver.eigenvectors());
	return result;
}
