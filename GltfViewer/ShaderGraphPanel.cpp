#include "ShaderGraphPanel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace Phantom::Gltf {
namespace {
using namespace Phmat;
struct Input { std::string name, id; };
std::vector<Input> inputs(const PhmatNode& n) {
    switch (n.type) {
    case NodeType::NormalMap: return {{"scale", n.normalMapScale}};
    case NodeType::Mix: return {{"a", n.mixA}, {"b", n.mixB}, {"factor", n.mixFactor}};
    case NodeType::Math: return {{"a", n.mathA}, {"b", n.mathB}};
    case NodeType::Custom: {
        std::vector<Input> result;
        for (size_t i = 0; i < n.customInputs.size(); ++i)
            result.push_back({"input " + std::to_string(i), n.customInputs[i]});
        return result;
    }
    case NodeType::PbrOutput:
        return {{"baseColor", n.pbrBaseColor}, {"metallic", n.pbrMetallic},
                {"roughness", n.pbrRoughness}, {"normal", n.pbrNormal},
                {"occlusion", n.pbrOcclusion}, {"emissive", n.pbrEmissive}, {"alpha", n.pbrAlpha}};
    default: return {};
    }
}
const char* typeName(NodeType t) {
    switch (t) {
    case NodeType::Constant: return "Constant";
    case NodeType::TextureSlot: return "Texture Slot";
    case NodeType::Uv: return "UV";
    case NodeType::NormalMap: return "Normal Map";
    case NodeType::Mix: return "Mix";
    case NodeType::Math: return "Math";
    case NodeType::Custom: return "Custom GLSL";
    case NodeType::PbrOutput: return "PBR Output";
    }
    return "Unknown";
}
const char* valueTypeName(ValueType t) {
    switch (t) {
    case ValueType::Float: return "float";
    case ValueType::Vec2: return "vec2";
    case ValueType::Vec3: return "vec3";
    case ValueType::Vec4: return "vec4";
    }
    return "?";
}
std::string detail(const PhmatNode& node) {
    if (node.type == NodeType::Constant) {
        char text[160];
        const auto v = node.constantValue;
        switch (node.constantType) {
        case ValueType::Float: std::snprintf(text, sizeof(text), "%.4g", v.x); break;
        case ValueType::Vec2: std::snprintf(text, sizeof(text), "(%.4g, %.4g)", v.x, v.y); break;
        case ValueType::Vec3: std::snprintf(text, sizeof(text), "(%.4g, %.4g, %.4g)", v.x, v.y, v.z); break;
        case ValueType::Vec4: std::snprintf(text, sizeof(text), "(%.4g, %.4g, %.4g, %.4g)", v.x, v.y, v.z, v.w); break;
        }
        return text;
    }
    if (node.type == NodeType::Uv) return "UV set " + std::to_string(node.uvSet);
    if (node.type == NodeType::Custom) return node.customPhshaderPath;
    if (node.type == NodeType::TextureSlot) {
        switch (node.textureSlot) {
        case TextureSlot::BaseColor: return "baseColor";
        case TextureSlot::MetallicRoughness: return "metallicRoughness";
        case TextureSlot::Normal: return "normal";
        case TextureSlot::Occlusion: return "occlusion";
        case TextureSlot::Emissive: return "emissive";
        }
    }
    if (node.type == NodeType::Math) {
        switch (node.mathOp) {
        case MathOp::Add: return "add";
        case MathOp::Subtract: return "subtract";
        case MathOp::Multiply: return "multiply";
        case MathOp::Divide: return "divide";
        case MathOp::Min: return "min";
        case MathOp::Max: return "max";
        }
    }
    return "";
}
float height(const PhmatNode& n) { return 86.f + 22.f * static_cast<float>(inputs(n).size()); }
}

void ShaderGraphPanel::setSource(int index, const std::string& path, const Phmat::PhmatLoadResult& result) {
    Source source;
    source.path = path;
    source.graph = result.graph;
    source.diagnostics = result.diagnostics;
    source.applied = result.success;
    source.parsed = result.graphParsed;
    // Longest dependency depth gives a deterministic left-to-right layout.
    // Invalid/cyclic graphs fall back to one column instead of recursing.
    std::vector<std::string> order;
    std::vector<Phmat::PhmatDiagnostic> unused;
    std::unordered_map<std::string, int> depths;
    if (Phmat::validateAndSort(source.graph, order, unused)) {
        std::unordered_map<std::string, const Phmat::PhmatNode*> nodes;
        for (const auto& node : source.graph.nodes) nodes.emplace(node.id, &node);
        for (const auto& id : order) {
            const auto& node = *nodes.at(id);
            std::string type;
            switch (node.type) {
            case NodeType::Constant: type = valueTypeName(node.constantType); break;
            case NodeType::TextureSlot: type = "vec4"; break;
            case NodeType::Uv: type = "vec2"; break;
            case NodeType::NormalMap: type = "vec3"; break;
            case NodeType::Custom: type = valueTypeName(node.customOutputType); break;
            case NodeType::Mix: type = source.outputTypes[node.mixA]; break;
            case NodeType::Math: type = source.outputTypes[node.mathA]; break;
            case NodeType::PbrOutput: type = "surface"; break;
            }
            source.outputTypes[id] = type;
            int depth = 0;
            for (const auto& input : inputs(*nodes.at(id)))
                if (!input.id.empty()) depth = std::max(depth, depths[input.id] + 1);
            depths[id] = depth;
        }
    }
    std::unordered_map<int, float> rows;
    for (const auto& node : source.graph.nodes) {
        const int depth = depths[node.id];
        source.positions.push_back(ImVec2(280.f * depth, rows[depth]));
        rows[depth] += height(node) + 30.f;
    }
    sources_[index] = std::move(source);
    if (index == materialIndex_) fit_ = true;
}

void ShaderGraphPanel::onImGui() {
    if (!visible_) return;
    ImGui::SetNextWindowPos(ImVec2(380, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(890, 680), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shader Graph", &visible_)) { ImGui::End(); return; }
    if (ImGui::InputInt("Material Index", &materialIndex_)) {
        materialIndex_ = std::max(0, materialIndex_); selected_.clear(); fit_ = true;
    }
    auto it = sources_.find(materialIndex_);
    if (it == sources_.end()) {
        ImGui::TextWrapped("No .phmat source loaded for this material. Load one in View > Material.");
        ImGui::End(); return;
    }
    auto& source = it->second;
    ImGui::TextWrapped("%s", source.path.c_str());
    ImGui::TextWrapped("%s", source.applied ? "Source applied to the material" :
        "Source not applied: renderer keeps its previous shader");
    if (ImGui::Button("Fit Graph")) fit_ = true;
    ImGui::SameLine(); ImGui::TextDisabled("Middle drag: pan | Wheel: zoom | Click: inspect");
    for (const auto& diagnostic : source.diagnostics)
        ImGui::TextWrapped("%s [%s]: %s", diagnostic.severity == Phmat::PhmatDiagnostic::Severity::Error ? "Error" : "Warning",
            diagnostic.nodeId.c_str(), diagnostic.message.c_str());
    for (const auto& node : source.graph.nodes) if (node.id == selected_) {
        ImGui::TextWrapped("Selected: %s (%s)", node.id.c_str(), typeName(node.type));
        ImGui::TextWrapped("%s", detail(node).c_str());
        for (size_t i = 0; i < node.customInputTypes.size(); ++i)
            ImGui::Text("Input %zu: %s", i, valueTypeName(node.customInputTypes[i]));
    }
    if (!source.parsed) ImGui::TextWrapped("Incomplete source: only nodes parsed before the error are shown.");
    ImGui::BeginChild("GraphCanvas", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 1.f); size.y = std::max(size.y, 1.f);
    ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    if (fit_) {
        float width = 250.f, extent = 100.f;
        for (size_t i = 0; i < source.positions.size(); ++i) {
            width = std::max(width, source.positions[i].x + 250.f);
            extent = std::max(extent, source.positions[i].y + height(source.graph.nodes[i]));
        }
        zoom_ = std::clamp(std::min((size.x - 40.f) / width, (size.y - 40.f) / extent), .2f, 1.5f);
        pan_ = ImVec2(20, 20); fit_ = false;
    }
    const auto& io = ImGui::GetIO();
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        pan_.x += io.MouseDelta.x; pan_.y += io.MouseDelta.y;
    }
    if (hovered && io.MouseWheel != 0.f) {
        const float next = std::clamp(zoom_ * std::pow(1.15f, io.MouseWheel), .2f, 2.5f);
        const ImVec2 mouse(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
        pan_ = ImVec2(mouse.x - (mouse.x - pan_.x) * next / zoom_, mouse.y - (mouse.y - pan_.y) * next / zoom_);
        zoom_ = next;
    }
    const auto point = [&](float x, float y) { return ImVec2(origin.x + pan_.x + x * zoom_, origin.y + pan_.y + y * zoom_); };
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    std::unordered_map<std::string, size_t> indices;
    for (size_t i = 0; i < source.graph.nodes.size(); ++i) indices.emplace(source.graph.nodes[i].id, i);
    for (size_t i = 0; i < source.graph.nodes.size(); ++i) {
        const auto pins = inputs(source.graph.nodes[i]);
        for (size_t j = 0; j < pins.size(); ++j) {
            auto from = indices.find(pins[j].id);
            if (from == indices.end()) continue;
            const auto a = source.positions[from->second], b = source.positions[i];
            const auto start = point(a.x + 250, a.y + 44), end = point(b.x, b.y + 86 + 22 * static_cast<float>(j));
            draw->AddBezierCubic(start, ImVec2(start.x + 60 * zoom_, start.y),
                ImVec2(end.x - 60 * zoom_, end.y), end, IM_COL32(110, 190, 240, 255), 2.f);
        }
    }
    for (size_t i = 0; i < source.graph.nodes.size(); ++i) {
        const auto& node = source.graph.nodes[i]; const auto pos = source.positions[i];
        const auto a = point(pos.x, pos.y), b = point(pos.x + 250, pos.y + height(node));
        bool error = false;
        for (const auto& diagnostic : source.diagnostics)
            if (diagnostic.nodeId == node.id && diagnostic.severity == Phmat::PhmatDiagnostic::Severity::Error) error = true;
        draw->AddRectFilled(a, b, IM_COL32(38, 43, 53, 255), 5.f);
        draw->AddRect(a, b, error ? IM_COL32(255, 90, 80, 255) : node.id == selected_ ?
            IM_COL32(255, 210, 90, 255) : IM_COL32(100, 120, 140, 255), 5.f);
        const auto label = [&](float y, const std::string& text) {
            const ImVec4 clip(a.x + 5, a.y, b.x - 5, b.y);
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * zoom_, point(pos.x + 10, pos.y + y),
                IM_COL32(230, 235, 240, 255), text.c_str(), nullptr, 0, &clip);
        };
        const auto type = source.outputTypes.find(node.id);
        label(8, node.id); label(30, std::string(typeName(node.type)) + " : " +
            (type == source.outputTypes.end() ? "?" : type->second));
        label(50, detail(node));
        const auto pins = inputs(node);
        for (size_t j = 0; j < pins.size(); ++j) {
            const float y = 86 + 22 * static_cast<float>(j);
            draw->AddCircleFilled(point(pos.x, pos.y + y), 4 * zoom_, IM_COL32(110, 190, 240, 255));
            label(y - 8, pins[j].name + (pins[j].id.empty() ? " (default)" : " <- " + pins[j].id));
        }
        if (node.type != Phmat::NodeType::PbrOutput)
            draw->AddCircleFilled(point(pos.x + 250, pos.y + 44), 4 * zoom_, IM_COL32(110, 190, 240, 255));
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= a.x && io.MousePos.x <= b.x &&
            io.MousePos.y >= a.y && io.MousePos.y <= b.y) selected_ = node.id;
    }
    draw->PopClipRect();
    ImGui::EndChild(); ImGui::End();
}
}
