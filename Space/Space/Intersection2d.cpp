#include "Intersection2d.h"

#include "../../../CGLib/Math/Line2d.h"
#include "../../../CGLib/Math/Circle2d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

template<typename T>
std::vector<T> Intersection2d<T>::calculate(const Line2d<T>& line, const Circle2d<T>& circle, const T tolerance)
{
    const auto start = line.getStart();
    const auto dir = line.getDirection();
    const auto center = circle.getCenter();
    const auto radius = circle.getRadius();

    // 2次方程式の係数
    const auto a = glm::dot(dir, dir);
    const auto oc = start - center;
    const auto b = 2 * glm::dot(dir, oc);
    const auto c = glm::dot(oc, oc) - radius * radius;

    const auto discriminant = b * b - 4 * a * c;

    std::vector<T> result;
    if (discriminant < -tolerance) {
        // 交点なし
        return result;
    }
    else if (std::abs(discriminant) <= tolerance) {
        // 重解（接する）
        const auto t = -b / (2 * a);
        if (t >= -tolerance && t <= 1 + tolerance) {
            result.push_back(t);
        }
    }
    else {
        // 2点で交わる
        const auto sqrtD = std::sqrt(discriminant);
        const auto t1 = (-b - sqrtD) / (2 * a);
        const auto t2 = (-b + sqrtD) / (2 * a);
        if (t1 >= -tolerance && t1 <= 1 + tolerance) {
            result.push_back(t1);
        }
        if (t2 >= -tolerance && t2 <= 1 + tolerance) {
            result.push_back(t2);
        }
    }
    return result;
}


template class Phantom::Space::Intersection2d<float>;
template class Phantom::Space::Intersection2d<double>;