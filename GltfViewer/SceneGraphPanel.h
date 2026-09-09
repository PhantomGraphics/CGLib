#pragma once

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "../GltfRenderer/Gltf/GltfDocument.h"
#include "World.h"

namespace Phantom::Gltf {

class SceneGraphPanel : public ::VKG::IVkUIPanel {
public:
    void setDocument(const GltfDocument* document) { document_ = document; }
    void setSelectedNode(int* nodeIndex) { selectedNode_ = nodeIndex; }
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }
    void onImGui() override;

private:
    const GltfDocument* document_ = nullptr;
    bool visible_ = true;
    int* selectedNode_ = nullptr;
    World world_;
};

} // namespace Phantom::Gltf
