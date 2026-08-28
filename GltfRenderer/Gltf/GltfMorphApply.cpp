#include "GltfMorphApply.h"
#include "GltfAccessorView.h"

#include <algorithm>

namespace Phantom::Gltf {

std::vector<glm::vec3> applyMorphs(const GltfDocument& doc, const GltfPrimitive& prim,
                                    const std::vector<float>& weights)
{
    std::vector<glm::vec3> result;
    if (prim.positionAccessor < 0) return result;

    GltfAccessorView baseView(doc, prim.positionAccessor);
    const size_t vertCount = baseView.count();
    result.resize(vertCount);
    for (size_t i = 0; i < vertCount; ++i)
        result[i] = baseView.get<glm::vec3>(i);

    for (size_t t = 0; t < prim.targets.size(); ++t) {
        const float w = (t < weights.size()) ? weights[t] : 0.f;
        if (w == 0.f) continue;
        const int acc = prim.targets[t].positionAccessor;
        if (acc < 0) continue;

        GltfAccessorView deltaView(doc, acc);
        const size_t n = std::min(vertCount, deltaView.count());
        for (size_t i = 0; i < n; ++i)
            result[i] += w * deltaView.get<glm::vec3>(i);
    }
    return result;
}

} // namespace Phantom::Gltf
