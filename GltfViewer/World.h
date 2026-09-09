#pragma once

#include "WorldPanel.h"

namespace Phantom::Gltf {

// The glTF world shown by the scene graph.  The implementation is kept in its
// own translation unit so the scene graph panel only coordinates its sections.
class World : public WorldPanel {
public:
    void onImGui(const GltfDocument& document, int& selectedNode) const;
};

} // namespace Phantom::Gltf
