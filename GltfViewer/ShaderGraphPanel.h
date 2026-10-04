#pragma once

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "../GltfRenderer/Phmat/PhmatCompiler.h"
#include "imgui.h"
#include <unordered_map>

namespace Phantom::Gltf {

// Read-only source inspector. View state never changes the material graph.
class ShaderGraphPanel : public ::VKG::IVkUIPanel {
public:
    void setVisible(bool value) { visible_ = value; }
    bool isVisible() const { return visible_; }
    void selectMaterial(int index) { if (materialIndex_ != index) { fit_ = true; selected_.clear(); } materialIndex_ = index; }
    void setSource(int index, const std::string& path, const Phmat::PhmatLoadResult& result);
    void clearMaterial(int index) { sources_.erase(index); }
    void clear() { sources_.clear(); selected_.clear(); }
    void onImGui() override;

private:
    struct Source {
        std::string path;
        Phmat::PhmatGraph graph;
        std::vector<Phmat::PhmatDiagnostic> diagnostics;
        bool applied = false;
        bool parsed = false;
        std::vector<ImVec2> positions;
        std::unordered_map<std::string, std::string> outputTypes;
    };
    std::unordered_map<int, Source> sources_;
    int materialIndex_ = 0;
    bool visible_ = false;
    bool fit_ = true;
    float zoom_ = 1.f;
    ImVec2 pan_{20.f, 20.f};
    std::string selected_;
};

}
