#pragma once

// Node definition registry: the single source of truth for node types, sockets,
// defaults and descriptions. A UI builds its palette and property panels from
// this metadata; the evaluator drives evaluation through it.

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "GraphTypes.h"

namespace Phantom::GeometryNode {

// Evaluation context. Part of every cache key except `cancel`.
struct EvalContext {
    uint32_t seed = 0;
    Limits limits;
    const std::atomic<bool>* cancel = nullptr;  // polled between nodes; may be null
    // Evaluating the inside of a node group: the values of the group's interface inputs (read by GroupInput) and how
    // deep the group nesting is (a self-referencing group is cut off instead of recursing forever).
    const std::map<std::string, Value>* groupInputs = nullptr;
    int groupDepth = 0;

    bool cancelled() const { return cancel && cancel->load(std::memory_order_relaxed); }
};

struct SocketDef {
    std::string id;           // stable identifier used in links / params / JSON
    std::string displayName;
    SocketType type = SocketType::Float;
    Value defaultValue;       // used when unlinked and no parameter is set (non-geometry)
    bool required = false;    // must be linked (Geometry inputs)
    bool multi = false;       // accepts several links (ordered); inputs only
    std::string description;
};

// Handed to a node's evaluate function. Inputs are already resolved (linked value,
// else parameter, else default). Report failures through fail(); never throw.
class NodeEvalContext {
public:
    NodeEvalContext(const EvalContext& ctx, NodeId node) : eval_(ctx), node_(node) {}

    const EvalContext& eval() const { return eval_; }
    const Limits& limits() const { return eval_.limits; }
    const std::atomic<bool>* cancelFlag() const { return eval_.cancel; }
    uint32_t seed() const { return eval_.seed; }
    NodeId node() const { return node_; }

    float getFloat(const std::string& id) const;
    int32_t getInt(const std::string& id) const;
    bool getBool(const std::string& id) const;
    Vec3 getVec3(const std::string& id) const;
    GeometryPtr getGeometry(const std::string& id) const;                 // null if absent
    FieldPtr getField(const std::string& id) const;                       // null if absent (optional field inputs)
    std::vector<GeometryPtr> getGeometries(const std::string& id) const;  // multi inputs

    void setOutput(const std::string& id, Value v) { outputs_[id] = std::move(v); }
    void fail(DiagCode code, std::string message, std::string socket = {});
    bool failed() const { return failed_; }

    // Filled by the evaluator before the call / read after it.
    std::map<std::string, std::vector<Value>> inputs;
    const std::map<std::string, Value>& outputs() const { return outputs_; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

private:
    const Value* first(const std::string& id) const;

    const EvalContext& eval_;
    NodeId node_;
    std::map<std::string, Value> outputs_;
    std::vector<Diagnostic> diagnostics_;
    bool failed_ = false;
};

using NodeEvalFn = std::function<void(NodeEvalContext&)>;

struct NodeDefinition {
    std::string typeId;      // "Box"
    uint32_t version = 1;
    std::string displayName;
    std::string category;
    std::string description;
    std::vector<SocketDef> inputs;
    std::vector<SocketDef> outputs;
    bool isOutput = false;   // the graph result node (exactly one per graph)
    bool isGroupOutput = false;  // the result node of a group's inner graph (its outputs are the interface outputs)
    uint64_t contentHash = 0;    // part of the cache key for definitions whose behaviour is data (node groups)
    NodeEvalFn evaluate;

    const SocketDef* findInput(const std::string& id) const;
    const SocketDef* findOutput(const std::string& id) const;
};

class NodeRegistry {
public:
    // False (and no change) on a duplicate type id or missing evaluate function.
    bool add(NodeDefinition def);
    const NodeDefinition* find(const std::string& typeId) const;
    const std::vector<NodeDefinition>& all() const { return defs_; }

    // Box, Grid, Transform/Join Geometry, Float/Vector value, Output, plus the Field nodes (FieldNodes.h).
    static const NodeRegistry& builtin();

private:
    std::vector<NodeDefinition> defs_;
};

}  // namespace Phantom::GeometryNode
