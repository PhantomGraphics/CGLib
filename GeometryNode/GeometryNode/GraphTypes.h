#pragma once

// Phantom::GeometryNode -- graph data model (what is saved) and diagnostics.
//
// A Graph is plain data: nodes (type id + version + parameters), links between
// named sockets, and an editor layout that is kept apart from the computation
// (changing a layout never invalidates the evaluation cache). Unknown node types
// and unknown parameters are preserved verbatim so a file written by a newer
// build survives a load/save round trip in an older one.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "../../ThirdParty/nlohmann/json.hpp"
#include "Field.h"
#include "GeometryTypes.h"

namespace Phantom::GeometryNode {

using NodeId = uint64_t;  // 0 is "no node" and never a valid node id.

inline constexpr uint32_t kGraphSchemaVersion = 1;

// Field* are per-element expressions (Field.h), distinct from the single values Float/Bool/Vector3.
enum class SocketType { Geometry, Float, Int, Bool, Vector3, FieldFloat, FieldVector3, FieldBool };

inline bool isFieldType(SocketType t) {
    return t == SocketType::FieldFloat || t == SocketType::FieldVector3 || t == SocketType::FieldBool;
}

const char* toString(SocketType t);
bool socketTypeFromString(const std::string& s, SocketType& out);

// No implicit conversion exists between socket types; a conversion is an
// explicit node. monostate = "no value".
using Value = std::variant<std::monostate, GeometryPtr, float, int32_t, bool, Vec3, FieldPtr>;

// The SocketType a Value currently holds (false for monostate).
bool valueType(const Value& v, SocketType& out);

// Parameter <-> JSON. Float: number, Int: integer, Bool: bool, Vector3: [x,y,z].
// Geometry has no literal form. Both return false on a type/range mismatch
// (the JSON is left alone, so callers can keep it as an unknown parameter).
bool paramFromJson(SocketType type, const nlohmann::json& j, Value& out);
bool paramToJson(const Value& v, nlohmann::json& out);

struct Endpoint {
    NodeId node = 0;
    std::string socket;  // socket id from the node definition (stable across versions)
    bool operator==(const Endpoint& o) const { return node == o.node && socket == o.socket; }
};

struct Link {
    Endpoint from;  // output socket
    Endpoint to;    // input socket
};

struct Node {
    NodeId id = 0;
    std::string type;       // registry type id, e.g. "Box"
    uint32_t version = 1;   // node-definition version this node was authored with
    // Values of input sockets that are not linked. Anything the definition does
    // not know is kept as-is (and reported as a warning when evaluated).
    std::map<std::string, nlohmann::json> params;
    nlohmann::json extra = nlohmann::json::object();  // unknown node-level keys, preserved
};

struct NodeLayout {
    double x = 0.0;
    double y = 0.0;
};

struct Graph;

// Node groups (docs/todo/PLAN_geometry_node.md Phase 6). A group is a named sub-graph with an explicit interface; a
// "Group:<name>" node in the graph evaluates it like a function. The definition is plain data stored in the owning
// Graph's library (Graph::groups) and is immutable once made: the inner graph's GroupInput node provides the interface
// inputs as outputs, its GroupOutput node collects the interface outputs, and its Group nodes refer to the same
// (flat) library by name. See GraphGroup.h.
struct GroupSocket {
    std::string id;    // socket id on the Group node and on GroupInput / GroupOutput (letters, digits, underscore)
    std::string name;  // display name
    SocketType type = SocketType::Geometry;
    nlohmann::json defaultValue;  // value types only; null = the type's zero value
};

struct GroupDef {
    std::string name;
    std::vector<GroupSocket> inputs;
    std::vector<GroupSocket> outputs;
    std::shared_ptr<const Graph> graph;  // never null in a valid definition
};

// A public parameter: a name that stands for one unlinked input socket of one node, so a graph used as a
// reusable asset can be driven (overridden) per object without editing its structure. See GraphAsset.h.
struct ExposedParam {
    std::string name;
    NodeId node = 0;
    std::string socket;
};

struct Graph {
    uint32_t schema = kGraphSchemaVersion;
    // Context seed of every random field (see Field.h). Saved with the graph so results reproduce.
    uint32_t seed = 0;
    std::vector<Node> nodes;
    std::vector<Link> links;                 // order = order of multi-input sockets
    std::vector<ExposedParam> exposed;       // public parameters (see GraphAsset.h)
    std::vector<GroupDef> groups;            // node group library (see GraphGroup.h)
    std::map<NodeId, NodeLayout> layout;     // editor only; not part of any cache key
    nlohmann::json extra = nlohmann::json::object();  // unknown root keys, preserved

    const Node* findNode(NodeId id) const;
    Node* findNode(NodeId id);
};

// Builder helpers (ids are max+1, never reused within one Graph's lifetime of edits).
Node& addNode(Graph& g, const std::string& type, uint32_t version = 1);
void addLink(Graph& g, NodeId from, const std::string& fromSocket, NodeId to, const std::string& toSocket);
void removeNode(Graph& g, NodeId id);  // also drops its links and layout
bool setParam(Node& n, const std::string& socket, const Value& v);

// --- Diagnostics -----------------------------------------------------------

enum class Severity { Warning, Error };

enum class DiagCode {
    DuplicateNodeId,
    InvalidNodeId,
    UnknownNodeType,
    UnsupportedNodeVersion,
    UnknownSocket,
    UnknownParameter,
    InvalidParameter,
    TypeMismatch,
    MultipleLinks,
    MissingInput,
    Cycle,
    NoOutputNode,
    MultipleOutputNodes,
    LimitExceeded,
    InvalidGeometry,
    SingularTransform,
    FieldMismatch,  // a field could not be evaluated on the geometry it is applied to
    Cancelled,
    Upstream,  // not evaluated because an upstream node failed
    Internal,
};

const char* toString(DiagCode c);

struct Diagnostic {
    Severity severity = Severity::Error;
    DiagCode code = DiagCode::Internal;
    NodeId node = 0;      // 0 = graph level
    std::string socket;   // empty = whole node
    std::string message;
};

bool hasErrors(const std::vector<Diagnostic>& d);

}  // namespace Phantom::GeometryNode
