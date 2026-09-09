#pragma once

#include "../GltfRenderer/Gltf/GltfDocument.h"

namespace Phantom::Gltf {

// Draws the document's scene hierarchy from the glTF scene roots.
class WorldPanel {
public:
    void onImGui(const GltfDocument& document, int& selectedNode) const;
};

} // namespace Phantom::Gltf
