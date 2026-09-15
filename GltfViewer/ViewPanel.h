#pragma once

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include <glm/glm.hpp>

namespace Phantom::Gltf {

class GltfSceneRenderer;
class App;

class ViewPanel : public ::VKG::IVkUIPanel {
public:
    void setRenderer(GltfSceneRenderer* renderer) { renderer_ = renderer; }
    // Non-owning; only needed for the "Load HDRI..."/"Clear HDRI" buttons (real-HDRI loading
    // lives on the app, alongside envCubemap_ -- see App::loadEnvironmentHDR()'s comment).
    void setApp(App* app) { app_ = app; }
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }
    void onImGui() override;

private:
    GltfSceneRenderer* renderer_ = nullptr;
    App* app_ = nullptr;
    bool visible_ = true;
    glm::vec3 lightPos_ = { 1.f, 2.f, 1.f };
    glm::vec3 lightColor_ = { 1.f, 1.f, 1.f };
    float lightIntensity_ = 3.f;
    bool useIBL_ = true;
};

} // namespace Phantom::Gltf
