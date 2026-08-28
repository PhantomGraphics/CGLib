#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>

#include "PolygonSampler.h"


#include "../../../CGLib/Math/Vector3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

    /**
     * レイキャスティングアルゴリズムによる三角形（面）との交差判定
     *
     * 点 P から Z軸正方向（レイ）に発射し、レイが三角形と交差するか判定する。
     * ここでは Möller–Trumbore アルゴリズムの厳密実装を使用します。
     *
     * @param p 判定したい点 (レイの始点)
     * @param face チェックする面（三角形）
     * @return bool 交差すれば true
     */
    bool PolygonSampler::rayIntersectsTriangle(const Vector3df& p, const Face& face) const
    {
        const auto& v0 = vertices[face.v1];
        const auto& v1 = vertices[face.v2];
        const auto& v2 = vertices[face.v3];

        // Möller–Trumbore 用の小ヘルパー
        auto cross = [](const Vector3df& a, const Vector3df& b) -> Vector3df {
            return Vector3df{ a.y * b.z - a.z * b.y,
                              a.z * b.x - a.x * b.z,
                              a.x * b.y - a.y * b.x };
        };
        auto dot = [](const Vector3df& a, const Vector3df& b) -> double {
            return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y + static_cast<double>(a.z) * b.z;
        };

        const double EPS = 1e-8;
        // レイ方向は +Z
        Vector3df dir{ 0.0f, 0.0f, 1.0f };

        // 三角形の辺
        Vector3df edge1{ v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
        Vector3df edge2{ v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };

        Vector3df h = cross(dir, edge2);
        double a = dot(edge1, h);

        // ほぼ平行（a ≈ 0）の場合は特殊処理：
        // レイが三角形面と平行か、レイ方向が面に平行なケース。
        if (std::fabs(a) < EPS) {
            // レイと三角形が同一平面上にあるか（面の法線が Z 軸に垂直でない限り稀）
            // ここでは、点 p が三角形の面上にあるかを XY 投影で判定する。
            // 面が厳密にレイ方向に平行な場合（垂直面など）には、この簡易判定で誤判定を避ける。
            // plane z の近さをチェック
            double plane_z = v0.z; // 三角形は平面上 -> 代表点の z を使用
            if (std::fabs(p.z - plane_z) > EPS) return false;

            // XY 平面への 2D 点内判定（バリセンティック）
            // Compute barycentric coordinates in XY
            double denom = (edge1.x * edge2.y - edge2.x * edge1.y);
            if (std::fabs(denom) < EPS) return false; // 退化三角形
            double invDenom = 1.0 / denom;
            // Vector from v0 to p in XY
            double px = p.x - v0.x;
            double py = p.y - v0.y;
            // Solve for u,v in 2D (edge1 * u + edge2 * v = p - v0)
            double u = (px * edge2.y - edge2.x * py) * invDenom;
            double v = (edge1.x * py - px * edge1.y) * invDenom;
            if (u >= -EPS && v >= -EPS && (u + v) <= 1.0 + EPS) {
                return true;
            }
            return false;
        }

        double f = 1.0 / a;
        Vector3df s{ static_cast<float>(p.x - v0.x), static_cast<float>(p.y - v0.y), static_cast<float>(p.z - v0.z) };
        double u = f * dot(s, h);
        if (u < -EPS || u > 1.0 + EPS) return false;

        Vector3df q = cross(s, edge1);
        double v = f * dot(dir, q);
        if (v < -EPS || (u + v) > 1.0 + EPS) return false;

        double t = f * dot(edge2, q);

        // t はレイパラメータ（dir が単位ベクトルなので z 差に相当）
        // t > EPS: レイ上の正の位置で交差
        // fabs(t) <= EPS: 点が面上（境界扱い） -> 内部と見なす
        if (t > EPS) {
            return true;
        }
        if (std::fabs(t) <= EPS) {
            // レイ始点が三角形面上（面上の点を内部とするため true）
            return true;
        }

        return false;
    }

    /**
     * 多面体内部判定 (レイキャスティングアルゴリズム)
     * @param p 判定したい点
     * @return bool 点が内部にあれば true
     */
    bool PolygonSampler::isInside(const Vector3df& p) const {
        // 多面体（ポリヘドロン）の内部判定は、点 P から発射したレイが面と交差する回数を数えます。
        // レイが Z 軸の正方向（P.z + infinity）に伸びると仮定します。
        int intersection_count = 0;

        for (const auto& face : faces) {
            if (rayIntersectsTriangle(p, face)) {
                intersection_count++;
            }
        }

        // 交差回数が奇数なら内部
        return intersection_count % 2 == 1;
    }


    /**
     * 等間隔の格子点（ボリュームグリッド）を多面体内部に生成するパブリックメソッド
     * @param spacing 格子点間の間隔
     * @return std::vector<Point3D> 多面体内部に生成された点のリスト
     */
    std::vector<Vector3df> PolygonSampler::generateUniformGrid(double spacing) const
    {
        std::vector<Vector3df> inside_points;
        if (vertices.empty() || faces.empty()) return inside_points;

        // 1. バウンディングボックスの計算
        float x_min = vertices[0].x, x_max = vertices[0].x;
        float y_min = vertices[0].y, y_max = vertices[0].y;
        float z_min = vertices[0].z, z_max = vertices[0].z;

        for (const auto& p : vertices) {
            x_min = std::min(x_min, p.x);
            x_max = std::max(x_max, p.x);
            y_min = std::min(y_min, p.y);
            y_max = std::max(y_max, p.y);
            z_min = std::min(z_min, p.z);
            z_max = std::max(z_max, p.z);
        }

        // 2. 3Dグリッドの生成と内部判定（三重ループ）
        for (double x = x_min; x <= x_max; x += spacing) {
            for (double y = y_min; y <= y_max; y += spacing) {
                for (double z = z_min; z <= z_max; z += spacing) {
                    Vector3df current_point = { x, y, z };

                    // 3. 内部判定を行い、内部にあれば採用
                    if (isInside(current_point)) {
                        inside_points.push_back(current_point);
                    }
                }
            }
        }

        return inside_points;
    }