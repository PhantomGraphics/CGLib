#pragma once

#include "GltfDocument.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace Phantom::Gltf {

    // Axis-aligned bounding box in the glTF document's own local space (i.e. before any
    // *external* transform, such as a Universe entity's UniverseTransform, is applied --
    // matching GltfSceneRenderer::setModelMatrix()'s "instance transform is separate from the
    // document" split).
    struct GltfAabb {
        glm::vec3 min{ 0.f };
        glm::vec3 max{ 0.f };
        bool      valid = false; // false if the document has no POSITION-bearing primitive

        glm::vec3 center()      const { return (min + max) * 0.5f; }
        glm::vec3 halfExtents() const { return (max - min) * 0.5f; }
    };

    // Computes the AABB of every POSITION-bearing primitive reachable from doc's default
    // scene, walking the node hierarchy the same way GltfSceneRenderer::traverseNode() does
    // (TRS/matrix nodes, recursive children) and accumulating each vertex transformed by its
    // node's local-to-document matrix. Intended for rough physics-collider sizing (see
    // docs/todo/PLAN_cgstudio_universe_pipeline.md Phase 4) -- not a substitute for a real
    // convex hull or mesh collider.
    GltfAabb computeGltfBounds(const GltfDocument& doc);

}
