#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Phantom::Texture {
using Color = std::array<float, 4>; // Straight alpha, linear RGB.
enum class NodeType { Solid, Noise, Gradient, Checker, Blend, BaseColor, Paint, Perlin, Mandelbrot, Julia };
struct NoiseSettings {
    unsigned mode = 0, octaves = 5;
    float lacunarity = 2, gain = 0.5f, ridgeOffset = 1, ridgeFeedback = 2;
    bool operator==(const NoiseSettings&) const = default;
};
struct EscapeSettings {
    double centerReal = -0.5, centerImaginary = 0, width = 3.5;
    unsigned iterations = 256;
    double constantReal = -0.8, constantImaginary = 0.156;
    bool operator==(const EscapeSettings&) const = default;
};
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
    NoiseSettings noise;
    EscapeSettings escape;
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
struct ByteImage { unsigned width = 0, height = 0; std::vector<uint8_t> pixels; };
// Failure preserves destination objects. Cancellation is checked once per row.
bool validate(const Recipe&, std::string& diagnostic);
bool evaluate(const Recipe&, Image&, std::string& diagnostic, const std::atomic_bool* cancel = nullptr);
// Packed straight-alpha sRGB bytes. blueFirst selects BGRA for display consumers.
// Uses the same evaluation and encoding as PNG export without a float result copy.
bool evaluateSrgb8(const Recipe&, ByteImage&, std::string& diagnostic, bool blueFirst = false, const std::atomic_bool* cancel = nullptr);
// Optional memory-only BGRA8 display cache. Appended strokes update only their bounds;
// all other semantic edits rebuild. Not thread safe; callers serialize access.
class PreviewEvaluator {
public:
    PreviewEvaluator();
    ~PreviewEvaluator();
    PreviewEvaluator(const PreviewEvaluator&)=delete;
    PreviewEvaluator& operator=(const PreviewEvaluator&)=delete;
    bool evaluate(const Recipe&,ByteImage&,std::string&,const std::atomic_bool* cancel=nullptr);
private:
    struct State;
    std::unique_ptr<State> state_;
};
bool serialize(const Recipe&, std::string& json, std::string& diagnostic);
bool deserialize(const std::string& json, Recipe&, std::string& diagnostic);
bool save(const std::filesystem::path&, const Recipe&, std::string& diagnostic);
bool load(const std::filesystem::path&, Recipe&, std::string& diagnostic);
bool writePng(const std::filesystem::path&, const Image&, std::string& diagnostic);
Recipe example();
}
