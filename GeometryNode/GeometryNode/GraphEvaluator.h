#pragma once

// Graph validation and CPU evaluation.
//
// The entire graph is checked for structural problems (including disconnected cycles).
// Only the part that the (single) Output node depends on is checked for per-node problems
// and evaluated, in topological order. Results
// are immutable and shared. An EvalCache lets a caller re-evaluate after an edit
// and recompute only the changed nodes and what is downstream of them.
//
// Exception-free: success and node/socket-level diagnostics are returned.

#include <map>
#include <string>
#include <vector>

#include "GraphTypes.h"
#include "NodeRegistry.h"

namespace Phantom::GeometryNode {

struct EvalStats {
    size_t nodesEvaluated = 0;  // node functions actually run
    size_t nodesCached = 0;     // results reused from the cache
};

struct EvalResult {
    bool success = false;
    GeometryPtr geometry;                 // non-null iff success (null for the inside of a node group)
    std::map<std::string, Value> values;  // outputs of the result node (a group's interface outputs)
    NodeId outputNode = 0;
    std::vector<Diagnostic> diagnostics;  // warnings may be present on success
    EvalStats stats;
};

// Per-node results of the previous evaluation. Keys are exact (node type/version,
// resolved parameter values, upstream revisions, seed, limits), so a hit is always
// a value that would be recomputed identically. Layout edits never touch it.
class EvalCache {
public:
    void clear() { entries.clear(); }
    size_t size() const { return entries.size(); }

    // Internal to the evaluator (public so the .cpp helpers can use it).
    struct Entry {
        std::string key;
        uint64_t revision = 0;
        std::map<std::string, Value> outputs;
    };
    std::map<NodeId, Entry> entries;
    uint64_t nextRevision = 1;
};

// Structural + per-node checks without evaluating anything. Errors: duplicate /
// zero ids, links to missing nodes or sockets, type mismatch, several links into a
// single-link input, missing or multiple Output nodes, cycles, and (for nodes the
// Output depends on) unknown types, unsupported versions, invalid parameters and
// missing required inputs. Unknown parameters are warnings.
std::vector<Diagnostic> validateGraph(const Graph& graph, const NodeRegistry& registry);

// validateGraph() first; evaluation only runs if there are no errors. `cache` may be null.
EvalResult evaluateGraph(const Graph& graph, const NodeRegistry& registry, const EvalContext& context,
                         EvalCache* cache = nullptr);

}  // namespace Phantom::GeometryNode
