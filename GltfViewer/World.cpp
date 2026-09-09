#include "World.h"

namespace Phantom::Gltf {

void World::onImGui(const GltfDocument& document, int& selectedNode) const {
    WorldPanel::onImGui(document, selectedNode);
}

} // namespace Phantom::Gltf
