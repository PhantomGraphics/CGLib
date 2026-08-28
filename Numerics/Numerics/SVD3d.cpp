#include "pch.h"

#include "Converter.h"
#include "SVD3d.h"

using namespace Phantom::Math;
using namespace Phantom::Numerics;

SVD3d::Result SVD3d::calculate(const Matrix3dd& lhs)
{
	SVD3d::Result result;
	Eigen::Matrix3d A = Converter::toEigen(lhs);
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eigensolver(A);
	if (eigensolver.info() != Eigen::Success) {
		result.isOk = false;
	}
	result.isOk = true;
	result.eigenValues = Converter::fromEigen(eigensolver.eigenvalues());
	result.eigenVectors = Converter::fromEigen(eigensolver.eigenvectors());
	return result;
}

SVD3d::Result SVD3d::calculateJacobi(const Matrix3dd& matrix)
{
	SVD3d::Result result;
	Eigen::Matrix3d A = Converter::toEigen(matrix);
	Eigen::JacobiSVD<Eigen::Matrix3d> svd(A, Eigen::ComputeFullU);
	const auto& R = svd.matrixU();
	result.isOk = true;
	result.eigenValues = Converter::fromEigen(svd.singularValues());
	result.eigenVectors = Converter::fromEigen(svd.matrixU());
	return result;
}

SVD3d::FullResult SVD3d::calculateFullJacobi(const Matrix3dd& matrix)
{
	SVD3d::FullResult result;
	Eigen::Matrix3d A = Converter::toEigen(matrix);
	Eigen::JacobiSVD<Eigen::Matrix3d> svd(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
	result.isOk = true;
	result.singularValues = Converter::fromEigen(svd.singularValues());
	result.matrixU = Converter::fromEigen(svd.matrixU());
	result.matrixV = Converter::fromEigen(svd.matrixV());
	return result;
}