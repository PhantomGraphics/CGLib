#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <algorithm>
#include <vector>

#include "../Gltf/GltfDocument.h"

namespace Phantom::Gltf {

    // CPU-only helpers GltfSceneRenderer uses while building and drawing primitives, split out so
    // they can be tested without a GPU.

    // Centre of the axis-aligned box of a POSITION accessor (accessor space). Returns (0,0,0) for
    // an invalid or empty accessor.
    glm::vec3 accessorAabbCenter(const GltfDocument& doc, int accessorIndex);

    // Accessor-space positions and normals of a primitive, for per-frame re-baking. A primitive
    // without normals gets (0,1,0) for every vertex. Outputs are cleared first.
    void readPrimitiveGeometry(const GltfDocument& doc, const GltfPrimitive& prim,
                               std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals);

    // Sorts alpha-BLEND draw items farthest-first from `eye` (painter's algorithm). `Item` must
    // expose `restWorld` (glm::mat4) and `localCenter` (glm::vec3); the sort key is the item's
    // centre transformed by model * restWorld. Not stable for equal distances.
    template <typename ItemPtr>
    void sortFarthestFirst(std::vector<ItemPtr>& items, const glm::mat4& model, const glm::vec3& eye)
    {
        auto distSq = [&](const ItemPtr& it) {
            const glm::vec3 w = glm::vec3(model * it->restWorld * glm::vec4(it->localCenter, 1.f));
            const glm::vec3 d = w - eye;
            return glm::dot(d, d);
        };
        std::sort(items.begin(), items.end(),
                  [&](const ItemPtr& a, const ItemPtr& b) { return distSq(a) > distSq(b); });
    }

}
