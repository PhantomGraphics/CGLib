#pragma once

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "../GltfRenderer/Gltf/GltfDocument.h"

namespace Phantom::Gltf {

class SceneGraphPanel : public ::VKG::IVkUIPanel {
public:
    void setDocument(const GltfDocument* document) { document_ = document; }
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }
    void onImGui() override;

private:
    const GltfDocument* document_ = nullptr;
    bool visible_ = true;
};

} // namespace Phantom::Gltf
