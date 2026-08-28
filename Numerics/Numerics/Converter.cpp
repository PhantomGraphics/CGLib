#include "pch.h"

#include "Converter.h"

using namespace Phantom::Math;
using namespace Phantom::Numerics;

Eigen::Matrix2d Converter::toEigen(const Matrix2dd& src)
{
	Eigen::Matrix2d dest;
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			dest(j, i) = src[i][j];
		}
	}
	return dest;
}

Eigen::Matrix3d Converter::toEigen(const Matrix3dd& src)
{
	Eigen::Matrix3d dest;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			dest(j, i) = src[i][j];
		}
	}
	return dest;
}

Eigen::Matrix4d Converter::toEigen(const Matrix4dd& src)
{
	Eigen::Matrix4d dest;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			dest(j, i) = src[i][j];
		}
	}
	return dest;
}

Eigen::Vector2d Converter::toEigen(const Vector2dd& src)
{
	return Eigen::Vector2d(src[0], src[1]);
}

Eigen::Vector3d Converter::toEigen(const Vector3dd& src)
{
	return Eigen::Vector3d(src[0], src[1], src[2]);
}

Eigen::Vector4d Converter::toEigen(const Vector4dd& src)
{
	return Eigen::Vector4d(src[0], src[1], src[2], src[3]);
}

Matrix2dd Converter::fromEigen(const Eigen::Matrix2d& src)
{
	Matrix2dd dest;
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			dest[j][i] = src(i, j);
		}
	}
	return dest;
}

Matrix3dd Converter::fromEigen(const Eigen::Matrix3d& src)
{
	Matrix3dd dest;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			dest[j][i] = src(i, j);
		}
	}
	return dest;
}

Matrix4dd Converter::fromEigen(const Eigen::Matrix4d& src)
{
	Matrix4dd dest;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			dest[j][i] = src(i, j);
		}
	}
	return dest;
}

Vector2dd Converter::fromEigen(const Eigen::Vector2d& src)
{
	return Vector2dd(src[0], src[1]);
}

Vector3dd Converter::fromEigen(const Eigen::Vector3d& src)
{
	return Vector3dd(src[0], src[1], src[2]);
}

Vector4dd Converter::fromEigen(const Eigen::Vector4d& src)
{
	return Vector4dd(src[0], src[1], src[2], src[3]);
}