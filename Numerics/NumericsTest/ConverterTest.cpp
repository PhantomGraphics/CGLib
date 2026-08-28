#include "pch.h"

#include "../Numerics/Converter.h"

using namespace Phantom::Math;
using namespace Phantom::Numerics;

namespace {
    constexpr double tolerance = 1.0e-12;
}

// toEigen(fromEigen(A)) == A  (round-trip)
TEST(ConverterTest, Matrix3dRoundTrip)
{
    Eigen::Matrix3d src;
    src << 1, 2, 3,
           4, 5, 6,
           7, 8, 9;

    const Matrix3dd mid = Converter::fromEigen(src);
    const Eigen::Matrix3d dst = Converter::toEigen(mid);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            EXPECT_NEAR(src(i, j), dst(i, j), tolerance)
                << "mismatch at (" << i << "," << j << ")";
        }
    }
}

// fromEigen preserves element values: glm[col][row] == Eigen(row,col)
TEST(ConverterTest, Matrix3dFromEigenElementOrder)
{
    Eigen::Matrix3d src;
    src << 1, 2, 3,
           4, 5, 6,
           7, 8, 9;

    const Matrix3dd m = Converter::fromEigen(src);

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // glm column-major: m[col][row]
            EXPECT_NEAR(src(row, col), m[col][row], tolerance)
                << "mismatch at row=" << row << " col=" << col;
        }
    }
}

// toEigen preserves element values: Eigen(row,col) == glm[col][row]
TEST(ConverterTest, Matrix3dToEigenElementOrder)
{
    // construct glm matrix with known values: m[col][row]
    Matrix3dd src;
    src[0][0] = 1; src[0][1] = 4; src[0][2] = 7;
    src[1][0] = 2; src[1][1] = 5; src[1][2] = 8;
    src[2][0] = 3; src[2][1] = 6; src[2][2] = 9;

    const Eigen::Matrix3d m = Converter::toEigen(src);

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            EXPECT_NEAR(src[col][row], m(row, col), tolerance)
                << "mismatch at row=" << row << " col=" << col;
        }
    }
}

// Matrix2d round-trip
TEST(ConverterTest, Matrix2dRoundTrip)
{
    Eigen::Matrix2d src;
    src << 1, 2,
           3, 4;

    const Matrix2dd mid = Converter::fromEigen(src);
    const Eigen::Matrix2d dst = Converter::toEigen(mid);

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            EXPECT_NEAR(src(i, j), dst(i, j), tolerance)
                << "mismatch at (" << i << "," << j << ")";
        }
    }
}

// Matrix4d round-trip
TEST(ConverterTest, Matrix4dRoundTrip)
{
    Eigen::Matrix4d src;
    src <<  1,  2,  3,  4,
            5,  6,  7,  8,
            9, 10, 11, 12,
           13, 14, 15, 16;

    const Matrix4dd mid = Converter::fromEigen(src);
    const Eigen::Matrix4d dst = Converter::toEigen(mid);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(src(i, j), dst(i, j), tolerance)
                << "mismatch at (" << i << "," << j << ")";
        }
    }
}
