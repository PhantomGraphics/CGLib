#pragma once

// Shader graph (docs/todo/PLAN_phantomstudio_shader_graph.md Phase 1).
//
// CPU-only foundation: graph model, node registry, validation, typed GLSL
// generation and JSON. No Vulkan / WPF / Studio dependency, no exceptions
// (failures are returned as diagnostics). The generated GLSL is a single
// function
//
//     void sg_evaluate(vec2 uv, mat3 tbn, out SGSurface s);
//
// plus its uniform declarations. Every scalar / colour value (value-node
// parameters and unconnected input defaults) lives in the std140 block
// `SGParams` at set 1 / binding 0, so editing a value never needs a recompile;
// textures are `sg_tex<N>` samplers at set 1 / binding N+1.

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../../ThirdParty/nlohmann/json.hpp"

namespace Phantom::ShaderGraph {

constexpr int kSchemaVersion = 1;
constexpr int kMaxNodes = 256;
constexpr int kMaxTextures = 8;
constexpr size_t kMaxGlslBytes = 256 * 1024;

// Color = linear RGB colour, Normal = world-space normal. They never convert
// implicitly; ToColor / ToVector are explicit.
enum class SocketType { Float, Vec2, Vec3, Vec4, Color, Normal };

const char* socketTypeName(SocketType t);
bool socketTypeFromName(const std::string& s, SocketType& out);
int socketComponents(SocketType t);
const char* glslTypeName(SocketType t);

using NodeId = int;
using Vec4f = std::array<float, 4>;

struct Node {
    NodeId id = 0;
    std::string type;
    int version = 1;
    std::map<std::string, nlohmann::json> params;
    nlohmann::json extra = nlohmann::json::object();  // unknown node-level keys, preserved
};

struct Link {
    NodeId fromNode = 0;
    std::string fromSocket;
    NodeId toNode = 0;
    std::string toSocket;
};

struct NodeLayout { double x = 0, y = 0; };

struct Graph {
    int schema = kSchemaVersion;
    std::vector<Node> nodes;
    std::vector<Link> links;
    std::map<NodeId, NodeLayout> layout;  // editor only: never affects compilation
    nlohmann::json extra = nlohmann::json::object();

    const Node* findNode(NodeId id) const;
    Node* findNode(NodeId id);
};

// ---- registry --------------------------------------------------------------

struct SocketDef {
    std::string name;
    SocketType type = SocketType::Float;
    Vec4f def{};              // default for unconnected / value-parameter use
    bool hasDefault = false;  // false: socket has a builtin source (e.g. uv) or is optional
};

struct NodeSignature {
    std::vector<SocketDef> inputs;
    std::vector<SocketDef> outputs;
    std::vector<SocketDef> valueParams;  // value-node payload (not linkable)
};

// All known node type names, in a stable order.
const std::vector<std::string>& registeredNodeTypes();
bool isKnownNodeType(const std::string& type);
// Resolves sockets for `node` (the Type param selects the data type of Add/Multiply/Mix).
bool resolveSignature(const Node& node, NodeSignature& out, std::string& error);

// ---- validation ------------------------------------------------------------

enum class Severity { Warning, Error };

struct Diagnostic {
    Severity severity = Severity::Error;
    NodeId node = 0;  // 0 = graph level
    std::string code;
    std::string message;
};

// Structural + type validation. Unknown node types are an error only when the
// node is reachable from the Surface Output (otherwise a warning).
std::vector<Diagnostic> validateGraph(const Graph& g);
bool hasErrors(const std::vector<Diagnostic>& d);

// ---- compilation -----------------------------------------------------------

struct TextureBinding {
    int index = 0;  // sg_tex<index>, binding index + 1
    NodeId node = 0;
    std::string path;
    bool srgb = true;  // colour texture: sample through an sRGB view (linearised by hardware)
};

struct ParamSlot {
    int slot = 0;  // vec4 index in SGParams
    NodeId node = 0;
    std::string name;  // param / unconnected-input name on the node
    SocketType type = SocketType::Float;
};

struct LineRange {
    NodeId node = 0;
    int firstLine = 0, lastLine = 0;  // 1-based, inclusive, in `glsl`
};

struct CompileResult {
    bool ok = false;
    std::string glsl;  // fragment-shader fragment (declarations + sg_evaluate)
    std::vector<TextureBinding> textures;
    std::vector<ParamSlot> params;
    std::vector<LineRange> lineMap;
    std::vector<Diagnostic> diagnostics;
    uint64_t cacheKey = 0;  // hash of glsl; value-only edits keep it
};

// Deterministic: the same graph structure yields the same text.
CompileResult compileGraph(const Graph& g);
// Packs current parameter values for result.params (4 floats per slot).
std::vector<float> packParameters(const Graph& g, const CompileResult& r);
// Maps a GLSL compiler error line back to a node (0 when outside any node).
NodeId nodeForGlslLine(const CompileResult& r, int line);

// ---- JSON ------------------------------------------------------------------

struct ParseResult {
    bool ok = false;
    std::string error;
};

nlohmann::json graphToJson(const Graph& g);
// Replaces `out` only on success. Rejects a schema newer than kSchemaVersion.
ParseResult graphFromJson(const nlohmann::json& j, Graph& out);
std::string serializeGraph(const Graph& g, int indent = 2);
ParseResult parseGraph(const std::string& text, Graph& out);

// Reads `components` floats from a JSON number or array into `out`.
bool vec4FromJson(const nlohmann::json& j, int components, Vec4f& out);

}  // namespace Phantom::ShaderGraph
