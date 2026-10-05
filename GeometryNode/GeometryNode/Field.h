#pragma once

// Phantom::GeometryNode -- attribute domains and Fields (docs/todo/PLAN_geometry_node.md Phase 4).
//
// A Field is an immutable per-element expression that is evaluated lazily by the node that
// consumes it (Set Position, Delete Geometry, ...), against that node's geometry. A Field is NOT a
// single value: a Vector3 and a "Vector3 field" are different socket types and never convert
// implicitly (a constant becomes a field through an explicit node: Float / Vector Field).
//
// Domains. Attributes live on a domain (Point, Edge, Face, FaceCorner, Instance). The MVP mesh only
// stores point-domain attributes (position, normal, uv), so fields are evaluated on Domain::Point;
// the other domains are reserved. No node converts between domains silently -- when one is added
// the conversion will be an explicit node.
//
// Seed contract (random fields). A random value is a pure function of
//     (EvalContext::seed, the node's Seed parameter, the element index)
// built only from integer arithmetic (randomHash below), so it is identical across platforms,
// builds and runs, and does not depend on evaluation order or on how many other fields exist.
// The context seed is the Graph's `seed`, which is saved with the graph.

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "GeometryTypes.h"

namespace Phantom::GeometryNode {

enum class Domain { Point, Edge, Face, FaceCorner, Instance };
const char* toString(Domain d);

enum class FieldType { Float, Vector3, Bool };

struct FieldContext {
    const Mesh* mesh = nullptr;
    uint32_t seed = 0;            // EvalContext::seed
    Domain domain = Domain::Point;

    // Number of elements of the domain (Point: vertices, Face: triangles, FaceCorner: indices).
    size_t size() const;
};

// One evaluation result; only the vector matching the Field's type is filled.
struct FieldData {
    std::vector<float> floats;
    std::vector<Vec3> vectors;
    std::vector<uint8_t> bools;
};

struct Field {
    FieldType type = FieldType::Float;
    std::function<void(const FieldContext&, FieldData&)> eval;
};
using FieldPtr = std::shared_ptr<const Field>;

// Evaluate helpers. A null or wrongly typed field yields an empty vector.
std::vector<float> evaluateFloats(const FieldPtr& f, const FieldContext& ctx);
std::vector<Vec3> evaluateVectors(const FieldPtr& f, const FieldContext& ctx);
std::vector<uint8_t> evaluateBools(const FieldPtr& f, const FieldContext& ctx);

FieldPtr makeFloatField(std::function<void(const FieldContext&, std::vector<float>&)> fn);
FieldPtr makeVectorField(std::function<void(const FieldContext&, std::vector<Vec3>&)> fn);
FieldPtr makeBoolField(std::function<void(const FieldContext&, std::vector<uint8_t>&)> fn);

// 32-bit hash of (context seed, node seed, element index): splitmix64 finalizer applied twice,
//   h = mix(mix(uint64(contextSeed) * 0x9E3779B97F4A7C15) ^ (uint64(uint32(nodeSeed)) << 32 | index))
// returning the high 32 bits. Frozen: changing it changes every saved random result
// (GraphEvaluatorTest/FieldTest pin golden values).
uint32_t randomHash(uint32_t contextSeed, int32_t nodeSeed, uint32_t index);
// [0, 1) from the hash: (h >> 8) / 2^24.
float randomUnit(uint32_t contextSeed, int32_t nodeSeed, uint32_t index);

}  // namespace Phantom::GeometryNode
