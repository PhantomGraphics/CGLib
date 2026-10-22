#include "PlanarTriangulation.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace Phantom::MeshEdit::detail {
bool triangulatePlanarBoundary(const std::vector<Math::Vector3dd>& p,
                              const Math::Vector3dd& normal, double epsilon,
                              size_t& work, std::vector<std::array<size_t, 3>>& triangles) {
    if (p.size() < 3) return false;
    std::vector<size_t> loop(p.size());
    std::iota(loop.begin(), loop.end(), size_t(0));
    std::vector<std::array<size_t, 3>> result;
    const double areaEpsilon = epsilon * epsilon;
    auto turn = [&](size_t a, size_t b, size_t c) {
        return glm::dot(glm::cross(p[b] - p[a], p[c] - p[a]), normal);
    };
    while (loop.size() > 3) {
        size_t best = loop.size();
        double quality = -1;
        for (size_t i = 0; i < loop.size(); ++i) {
            if (!work) return false;
            --work;
            const size_t a = loop[(i + loop.size() - 1) % loop.size()], b = loop[i],
                         c = loop[(i + 1) % loop.size()];
            const double area = turn(a, b, c);
            if (area <= areaEpsilon) continue;
            bool occupied = false;
            for (size_t q : loop) {
                if (!work) return false;
                --work;
                if (q == a || q == b || q == c) continue;
                // Include the ear boundary: skipping a collinear vertex would
                // create a T-junction when the neighboring patch retains it.
                if (turn(a, b, q) >= -areaEpsilon && turn(b, c, q) >= -areaEpsilon &&
                    turn(c, a, q) >= -areaEpsilon) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) continue;
            const double perimeter = glm::dot(p[b] - p[a], p[b] - p[a]) +
                                     glm::dot(p[c] - p[b], p[c] - p[b]) +
                                     glm::dot(p[a] - p[c], p[a] - p[c]);
            const double score = area / perimeter;
            if (score > quality) { best = i; quality = score; }
        }
        if (best == loop.size()) return false;
        result.push_back({loop[(best + loop.size() - 1) % loop.size()], loop[best],
                          loop[(best + 1) % loop.size()]});
        loop.erase(loop.begin() + best);
    }
    if (turn(loop[0], loop[1], loop[2]) <= areaEpsilon) return false;
    result.push_back({loop[0], loop[1], loop[2]});
    triangles = std::move(result);
    return true;
}
}
