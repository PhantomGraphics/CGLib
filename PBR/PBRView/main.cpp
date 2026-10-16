#include "CGLib/VkAppBase/VkAppBase.h"
#include "CGLib/VulkanGraphics/SpvResolver.h"
#include "CGLib/GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "CGLib/PBR/Core/GltfLight.h"
#include "CGLib/Math/Core/Vector3d.h"
#include "CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "CGLib/VkAppBase/ScenarioRunner/CommandQueue.h"
#include <nlohmann/json.hpp>
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>
#include <string_view>
#include <sstream>
#include <cstdio>

namespace {
using namespace Phantom;
using namespace Phantom::Gltf;
using Math::Vector3df;

template<class T>
int appendAccessor(GltfDocument& doc, const std::vector<T>& values,
                   GltfAccessorType type, GltfComponentType component) {
    GltfBuffer buffer;
    buffer.data.resize(values.size() * sizeof(T));
    std::memcpy(buffer.data.data(), values.data(), buffer.data.size());
    GltfBufferView view;
    view.bufferIndex = static_cast<int>(doc.buffers.size());
    view.byteLength = buffer.data.size();
    doc.buffers.push_back(std::move(buffer));
    GltfAccessor accessor;
    accessor.bufferViewIndex = static_cast<int>(doc.bufferViews.size());
    accessor.type = type;
    accessor.componentType = component;
    accessor.count = values.size();
    doc.bufferViews.push_back(view);
    doc.accessors.push_back(accessor);
    return static_cast<int>(doc.accessors.size()) - 1;
}

GltfDocument makeTestScene() {
    GltfDocument doc;
    std::vector<Vector3df> positions, normals;
    std::vector<uint32_t> indices;
    constexpr int slices = 48, stacks = 24;
    for (int y = 0; y <= stacks; ++y) {
        const float theta = PBR::kPi * y / stacks;
        for (int x = 0; x <= slices; ++x) {
            const float phi = 2.f * PBR::kPi * x / slices;
            const Vector3df n(std::sin(theta)*std::cos(phi), std::cos(theta),
                              std::sin(theta)*std::sin(phi));
            normals.push_back(n);
            positions.push_back(n * 0.78f);
        }
    }
    for (int y = 0; y < stacks; ++y)
        for (int x = 0; x < slices; ++x) {
            const uint32_t a = y * (slices + 1) + x, b = a + slices + 1;
            indices.insert(indices.end(), {a, a+1, b, a+1, b+1, b});
        }
    GltfPrimitive sphere;
    sphere.positionAccessor = appendAccessor(doc, positions, GltfAccessorType::Vec3, GltfComponentType::Float);
    sphere.normalAccessor = appendAccessor(doc, normals, GltfAccessorType::Vec3, GltfComponentType::Float);
    sphere.indicesAccessor = appendAccessor(doc, indices, GltfAccessorType::Scalar, GltfComponentType::UnsignedInt);
    GltfScene scene;
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 5; ++col) {
            GltfMaterial material;
            material.pbrMetallicRoughness.baseColorFactor = {0.9f, 0.46f, 0.16f, 1.f};
            material.pbrMetallicRoughness.metallicFactor = row / 4.f;
            material.pbrMetallicRoughness.roughnessFactor = 0.08f + 0.92f * col / 4.f;
            sphere.materialIndex = static_cast<int>(doc.materials.size());
            doc.materials.push_back(material);
            GltfMesh mesh;
            mesh.primitives.push_back(sphere);
            GltfNode node;
            node.meshIndex = static_cast<int>(doc.meshes.size());
            node.translation = {(col-2)*1.95f, (2-row)*1.95f, 0.f};
            scene.nodes.push_back(static_cast<int>(doc.nodes.size()));
            doc.meshes.push_back(std::move(mesh));
            doc.nodes.push_back(node);
        }
    doc.scenes.push_back(std::move(scene));
    return doc;
}

class PBRView final : public VKG::VkAppBase, public IScenarioDispatcher {
public:
    explicit PBRView(bool smoke) : VkAppBase(1440, 900, "PBRView"), document_(makeTestScene()) {
        add(&renderer_);
        if (smoke) setExitCondition([this] { return frames_ >= 12; });
    }
    bool shadersLoaded() const { return shadersLoaded_; }
    bool loadScenario(const std::string& path) {
        scenarioRequested_ = true;
        return runner_.load(path) && runner_.isActive();
    }
    bool scenarioPassed() const {
        return !scenarioRequested_ || (runner_.isFinished() && !runner_.hasFailed());
    }
    bool scenarioRunning() const { return runner_.isActive(); }
    void dispatch(const std::string& command) override { commands_.submit(command); }
    std::vector<std::string> collectResponses() override { return commands_.collectResponses(); }
protected:
    void onInit() override {
        GltfSceneRenderer::Shaders shaders;
        shaders.vertSpv = VKG::loadSPVRepo("shaders/gltf.vert.spv");
        shaders.fragSpv = VKG::loadSPVRepo("shaders/pbr.frag.spv");
        shadersLoaded_ = !shaders.vertSpv.empty() && !shaders.fragSpv.empty();
        renderer_.setShaders(std::move(shaders));
        renderer_.setDocument(document_);
        renderer_.setExtent(getExtent());
        renderer_.setUseIBL(false);
        VkAppBase::onInit();
    }
    void onSwapChainCreated() override { renderer_.setExtent(getExtent()); }
    void onUpdate(uint32_t frame) override {
        uint8_t pixel[4]{};
        if (pixelPending_ && pollPixelRead(pixel)) {
            commands_.respond(std::to_string(int(pixel[0]) + pixel[1] + pixel[2]));
            pixelPending_ = false;
        }
        std::string command;
        while (!pixelPending_ && commands_.takeOne(command)) {
            if (command == "ReadCenterBrightness") {
                requestPixelRead(getExtent().width / 2, getExtent().height / 2);
                pixelPending_ = true;
            } else commands_.respond(route(command));
        }
        if (runner_.isActive() && runner_.tick(*this, collectResponses())) {
            if (runner_.hasFailed())
                std::fprintf(stderr, "[Scenario] FAILED: %s\n", runner_.failMessage().c_str());
            else std::printf("[Scenario] PASSED (%zu steps)\n", runner_.stepCount());
        }
        const Vector3df eye(0.f, 0.f, distance_);
        const auto extent = getExtent();
        auto projection = glm::perspective(glm::radians(40.f), float(extent.width)/extent.height, 0.1f, 100.f);
        projection[1][1] *= -1.f;
        renderer_.setCamera(glm::lookAt(eye, Vector3df(0.f), Vector3df(0.f, 1.f, 0.f)), projection, eye);
        PBR::Light light;
        light.type = static_cast<PBR::LightType>(lightType_);
        light.position = lightPosition_;
        light.direction = glm::normalize(-lightPosition_);
        light.color = lightColor_;
        light.intensity = intensity_;
        light.spotAngleRad = glm::radians(cone_);
        renderer_.setPunctualLights({PBR::toLightEntry(light)});
        VkAppBase::onUpdate(frame);
        ++frames_;
    }
    void onImGui() override {
        // Anchor this single panel in its own reserved strip at every window size.
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({viewport->WorkPos.x + 12.f, viewport->WorkPos.y + 12.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({285.f, 360.f}, ImGuiCond_Always);
        ImGui::Begin("PBR test", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::BeginDisabled(runner_.isActive());
        ImGui::TextUnformatted("Columns: roughness 0.08 -> 1.0");
        ImGui::TextUnformatted("Rows: metallic 0.0 -> 1.0");
        ImGui::Separator();
        ImGui::Combo("Light", &lightType_, "Directional\0Point\0Spot\0");
        ImGui::SliderFloat("Intensity", &intensity_, 0.f, 100.f);
        ImGui::ColorEdit3("Color", &lightColor_.x);
        ImGui::SliderFloat3("Position", &lightPosition_.x, 1.f, 12.f);
        if (lightType_ == 2) ImGui::SliderFloat("Cone", &cone_, 10.f, 80.f);
        ImGui::SliderFloat("Distance", &distance_, 15.f, 30.f);
        if (ImGui::Button("Reset")) {
            reset();
        }
        ImGui::TextWrapped("Direct lighting only. Spot points at the grid center.");
        ImGui::EndDisabled();
        ImGui::End();
    }
private:
    void reset() {
        lightType_ = 0; intensity_ = 3.f; cone_ = 45.f;
        lightPosition_ = {4.f, 6.f, 8.f}; lightColor_ = Vector3df(1.f); distance_ = 19.f;
    }
    std::string route(const std::string& command) {
        if (command == "Reset") { reset(); return "OK"; }
        if (command == "GetState") {
            nlohmann::json state = {{"lightType", lightType_}, {"intensity", intensity_},
                {"cone", cone_}, {"distance", distance_},
                {"position", {lightPosition_.x, lightPosition_.y, lightPosition_.z}},
                {"color", {lightColor_.x, lightColor_.y, lightColor_.z}},
                {"sphereCount", document_.nodes.size()}, {"materialCount", document_.materials.size()},
                {"shadersLoaded", shadersLoaded_}};
            return nlohmann::json({{"ok", true}, {"result", state}}).dump();
        }
        std::istringstream args(command);
        std::string name, extra;
        args >> name;
        if (name == "SetLightType") {
            int value;
            if (!(args >> value) || (args >> extra) || value < 0 || value > 2) return "Error:invalid argument";
            lightType_ = value;
        } else if (name == "SetPosition" || name == "SetColor") {
            Vector3df value;
            if (!(args >> value.x >> value.y >> value.z) || (args >> extra)) return "Error:invalid argument";
            const float low = name == "SetPosition" ? 1.f : 0.f;
            const float high = name == "SetPosition" ? 12.f : 1.f;
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(value[i]) || value[i] < low || value[i] > high) return "Error:invalid argument";
            (name == "SetPosition" ? lightPosition_ : lightColor_) = value;
        } else if (name == "SetIntensity" || name == "SetCone" || name == "SetDistance") {
            float value;
            if (!(args >> value) || (args >> extra) || !std::isfinite(value)) return "Error:invalid argument";
            const float low = name == "SetCone" ? 10.f : name == "SetDistance" ? 15.f : 0.f;
            const float high = name == "SetCone" ? 80.f : name == "SetDistance" ? 30.f : 100.f;
            if (value < low || value > high) return "Error:invalid argument";
            (name == "SetCone" ? cone_ : name == "SetDistance" ? distance_ : intensity_) = value;
        } else return "Error:unknown command";
        return "OK";
    }
    ScenarioRunner runner_;
    CommandQueue commands_;
    bool scenarioRequested_ = false, pixelPending_ = false;
    GltfDocument document_;
    GltfSceneRenderer renderer_;
    Vector3df lightPosition_{4.f, 6.f, 8.f}, lightColor_{1.f};
    float intensity_ = 3.f, cone_ = 45.f, distance_ = 19.f;
    int lightType_ = 0, frames_ = 0;
    bool shadersLoaded_ = false;
};
}

int main(int argc, char* argv[]) {
    bool smoke = false;
    std::string scenario;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--smoke-test") smoke = true;
        else if (std::string_view(argv[i]) == "--run-scenario") {
            if (++i == argc) { std::fprintf(stderr, "--run-scenario requires a JSON path\n"); return 1; }
            scenario = argv[i];
        }
    }
    if (smoke && !scenario.empty()) {
        std::fprintf(stderr, "--smoke-test and --run-scenario cannot be combined\n"); return 1;
    }
    PBRView app(smoke);
    if (!scenario.empty()) {
        if (!app.loadScenario(scenario)) return 1;
        app.setExitCondition([&app] { return app.scenarioPassed() || !app.scenarioRunning(); });
    }
    return app.run(argc, argv) && app.shadersLoaded() && app.scenarioPassed() ? 0 : 1;
}
