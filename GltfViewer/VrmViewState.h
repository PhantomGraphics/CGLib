#pragma once

#include "../GltfRenderer/Vrm/VrmTypes.h"
#include <vector>

namespace Phantom::Gltf {

// UI-facing VRM state for GltfViewerApp's currently loaded document (default-constructed /
// inactive for a plain .gltf/.glb/.obj/.stl load). Free-standing rather than nested inside
// GltfViewerApp so GltfViewerPanel can reference it without including GltfViewerApp.h (which
// itself includes GltfViewerPanel.h -- nesting it in GltfViewerApp would be circular).
struct VrmViewState {
    bool           active = false;
    VrmSpecVersion specVersion = VrmSpecVersion::Unknown;
    VrmHumanoid    humanoid;
    std::vector<VrmExpression> expressions;
    std::vector<float>         expressionWeights; // one per expressions[], 0.0-1.0
    VrmMeta        meta;
};

}
