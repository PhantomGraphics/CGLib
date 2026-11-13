#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Phantom::Texture {
using Color = std::array<float, 4>; // Straight alpha, linear RGB.
enum class NodeType { Solid, Noise, Gradient, Checker, Blend, BaseColor, Paint };
struct Stroke {
    Color color{1, 0, 0, 1};
    float radius = 0.025f, strength = 1, hardness = 0.8f;
    bool erase = false;
    std::vector<std::array<float, 2>> samples; // UV; outside [-1,2] is rejected.
};
struct NodeInfo { NodeType type; const char* name; unsigned inputs; };
const std::vector<NodeInfo>& catalog();
struct Node {
    std::string id;
    NodeType type = NodeType::Solid;
    Color first{0, 0, 0, 1}, second{1, 1, 1, 1};
    uint32_t seed = 0;
    unsigned scale = 8;
    float x = 0, y = 0; // Editor placement, not evaluation coordinates.
    std::vector<Stroke> strokes;
};
struct Connection { std::string source, target; unsigned input = 0; };
struct Recipe {
    unsigned formatVersion = 1, evaluatorVersion = 1, resolution = 1024;
    std::vector<Node> nodes;
    std::vector<Connection> connections;
    std::string output;
};
struct Image { unsigned width = 0, height = 0; std::vector<Color> pixels; };
// Failure preserves destination objects. Cancellation is checked once per row.
bool validate(const Recipe&, std::string& diagnostic);
bool evaluate(const Recipe&, Image&, std::string& diagnostic, const std::atomic_bool* cancel = nullptr);
bool serialize(const Recipe&, std::string& json, std::string& diagnostic);
bool deserialize(const std::string& json, Recipe&, std::string& diagnostic);
bool save(const std::filesystem::path&, const Recipe&, std::string& diagnostic);
bool load(const std::filesystem::path&, Recipe&, std::string& diagnostic);
bool writePng(const std::filesystem::path&, const Image&, std::string& diagnostic);
Recipe example();
}
