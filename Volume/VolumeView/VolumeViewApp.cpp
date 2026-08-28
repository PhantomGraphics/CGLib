#include "VolumeViewApp.h"

namespace VkVolumeView {

VolumeViewApp::VolumeViewApp()
    : ::VKG::VkAppBase(1280, 720, "VkVolumeView")
{
    dispatcher_.setWorld(&world_);
    dispatcher_.setActiveSceneId(&activeSceneId_);
    dispatcher_.setActiveDenseSceneId(&activeDenseSceneId_);
    dispatcher_.setOnRebuild([this]() { notifyAll(); });
    dispatcher_.setPointRenderer(&pointRenderer_);
    dispatcher_.setDenseRenderer(&denseRenderer_);
    dispatcher_.setLineRenderer(&lineRenderer_);
    dispatcher_.setMenuPanel(&menuPanel_);
    dispatcher_.setPbvrRenderer(&pbvrRenderer_);
    dispatcher_.setApp(this);
    dispatcher_.setOnCameraReset([this]() {
        pointRenderer_.resetCamera();
        const auto cam = pointRenderer_.getCameraState();
        denseRenderer_.syncCamera(cam);
        lineRenderer_.syncCamera(cam);
        meshRenderer_.syncCamera(cam);
        pbvrRenderer_.syncCamera(cam.azimuth, cam.elevation, cam.distance);
    });
    scenarioBrowser_.setHost(this);
    scenarioBrowser_.setDefaultFolder("scenarios");

    add(&pointRenderer_);
    add(&denseRenderer_);
    add(&lineRenderer_);
    add(&pbvrRenderer_);
    add(&meshRenderer_);
    add(&menuPanel_);
    add(&scenarioBrowser_);
}

bool VolumeViewApp::loadScenario(const std::string& jsonPath) {
    return runner_.load(jsonPath);
}

void VolumeViewApp::onInit() {
    static constexpr auto kSh = "shaders/";
    {
        SparseVolumeRenderer::Shaders s;
        s.vertSpv = ::VKG::loadSPVRepo(std::string(kSh) + "volume_point.vert.spv");
        s.fragSpv = ::VKG::loadSPVRepo(std::string(kSh) + "volume_point.frag.spv");
        pointRenderer_.setShaders(std::move(s));
    }
    {
        DenseVolumeRenderer::Shaders s;
        s.vertSpv = ::VKG::loadSPVRepo(std::string(kSh) + "volume_point.vert.spv");
        s.fragSpv = ::VKG::loadSPVRepo(std::string(kSh) + "volume_point.frag.spv");
        denseRenderer_.setShaders(std::move(s));
    }
    {
        VectorFieldRenderer::Shaders s;
        s.vertSpv = ::VKG::loadSPVRepo(std::string(kSh) + "volume_line.vert.spv");
        s.fragSpv = ::VKG::loadSPVRepo(std::string(kSh) + "volume_line.frag.spv");
        lineRenderer_.setShaders(std::move(s));
    }
    {
        Phantom::Volume::PBVRRenderer::Shaders s;
        s.vertSpv = ::VKG::loadSPVRepo(std::string(kSh) + "pbvr_render.vert.spv");
        s.fragSpv = ::VKG::loadSPVRepo(std::string(kSh) + "pbvr_render.frag.spv");
        s.depositVertSpv = ::VKG::loadSPVRepo(std::string(kSh) + "opacity_shadow_deposit.vert.spv");
        s.depositFragSpv = ::VKG::loadSPVRepo(std::string(kSh) + "opacity_shadow_deposit.frag.spv");
        pbvrRenderer_.setShaders(std::move(s));
    }
    {
        MeshOverlayRenderer::Shaders s;
        s.vertSpv = ::VKG::loadSPVRepo(std::string(kSh) + "triangle.vert.spv");
        s.fragSpv = ::VKG::loadSPVRepo(std::string(kSh) + "triangle.frag.spv");
        meshRenderer_.setShaders(std::move(s));
    }

    ::VKG::VkAppBase::onInit();

    pointRenderer_.setWorld(&world_);
    denseRenderer_.setWorld(&world_);
    lineRenderer_.setWorld(&world_);
    pbvrRenderer_.setDataSource(&world_);
    meshRenderer_.setWorld(&world_);

    sceneListPanel_.init(&world_, &activeSceneId_, &activeDenseSceneId_,
        [this]() { notifyAll(); });

    menuPanel_.init(&world_, &activeSceneId_, &activeDenseSceneId_,
        [this]() { notifyAll(); },
        &sceneListPanel_,
        &pointRenderer_,
        &denseRenderer_,
        &lineRenderer_,
        &pbvrRenderer_,
        [this]() {
            pointRenderer_.resetCamera();
            const auto cam = pointRenderer_.getCameraState();
            denseRenderer_.syncCamera(cam);
            lineRenderer_.syncCamera(cam);
            meshRenderer_.syncCamera(cam);
            pbvrRenderer_.syncCamera(cam.azimuth, cam.elevation, cam.distance);
        });

    const auto ext = getExtent();
    pointRenderer_.setExtent(ext);
    denseRenderer_.setExtent(ext);
    lineRenderer_.setExtent(ext);
    pbvrRenderer_.setExtent(ext);
    meshRenderer_.setExtent(ext);

    denseRenderer_.syncCamera(pointRenderer_.getCameraState());
    lineRenderer_.syncCamera(pointRenderer_.getCameraState());
    meshRenderer_.syncCamera(pointRenderer_.getCameraState());
    const auto cam = pointRenderer_.getCameraState();
    pbvrRenderer_.syncCamera(cam.azimuth, cam.elevation, cam.distance);

    setupCallbacks();
}

void VolumeViewApp::onSwapChainCreated() {
    const auto ext = getExtent();
    pointRenderer_.setExtent(ext);
    denseRenderer_.setExtent(ext);
    lineRenderer_.setExtent(ext);
    pbvrRenderer_.setExtent(ext);
    meshRenderer_.setExtent(ext);
}

void VolumeViewApp::onUpdate(uint32_t frameIndex) {
    dispatcher_.processQueue();

    if (runner_.isActive()) {
        auto responses = dispatcher_.collectResponses();
        if (runner_.tick(dispatcher_, responses)) {
            if (runner_.hasFailed()) {
                fprintf(stderr, "[Scenario] FAILED: %s\n", runner_.failMessage().c_str());
                exitCode_ = 1;
            } else {
                fprintf(stdout, "[Scenario] PASSED (%zu steps)\n", runner_.stepCount());
                exitCode_ = 0;
            }
            if (exitOnComplete_) getWindow().close();
        }
    } else {
    }

    ::VKG::VkAppBase::onUpdate(frameIndex);
}

void VolumeViewApp::onPreRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    (void)frameIndex;
    pbvrRenderer_.renderShadowDeposit(cmd);
}

void VolumeViewApp::onImGui() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit"))
                glfwSetWindowShouldClose(getWindow().get(), GLFW_TRUE);
            ImGui::EndMenu();
        }
        menuPanel_.onImGuiMenuBar();
        ImGui::EndMainMenuBar();
    }
    ::VKG::VkAppBase::onImGui();
}

void VolumeViewApp::onCleanup() {
    ::VKG::VkAppBase::onCleanup();
}

void VolumeViewApp::notifyAll() {
    pointRenderer_.markDirty();
    denseRenderer_.markDirty();
    lineRenderer_.markDirty();
    pbvrRenderer_.markDirty();
    meshRenderer_.markDirty();
}

void VolumeViewApp::setupCallbacks() {
    auto& win = getWindow();

    win.onMouseButton = [this](int button, int action, int) {
        if (button == 0)
            pointRenderer_.handleMouseButton(action == 1);
    };

    win.onCursorPos = [this](double x, double y) {
        pointRenderer_.handleMouseMove(x, y);
        denseRenderer_.syncCamera(pointRenderer_.getCameraState());
        lineRenderer_.syncCamera(pointRenderer_.getCameraState());
        meshRenderer_.syncCamera(pointRenderer_.getCameraState());
        const auto cam = pointRenderer_.getCameraState();
        pbvrRenderer_.syncCamera(cam.azimuth, cam.elevation, cam.distance);
    };

    win.onScroll = [this](double, double yOffset) {
        pointRenderer_.handleScroll(yOffset);
        denseRenderer_.syncCamera(pointRenderer_.getCameraState());
        lineRenderer_.syncCamera(pointRenderer_.getCameraState());
        meshRenderer_.syncCamera(pointRenderer_.getCameraState());
        const auto cam = pointRenderer_.getCameraState();
        pbvrRenderer_.syncCamera(cam.azimuth, cam.elevation, cam.distance);
    };
}

} // namespace VkVolumeView
