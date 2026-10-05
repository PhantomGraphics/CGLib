#include "Field.h"

namespace Phantom::GeometryNode {

namespace {

uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    uint64_t z = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

}  // namespace

const char* toString(Domain d) {
    switch (d) {
        case Domain::Point: return "point";
        case Domain::Edge: return "edge";
        case Domain::Face: return "face";
        case Domain::FaceCorner: return "corner";
        case Domain::Instance: return "instance";
    }
    return "unknown";
}

size_t FieldContext::size() const {
    if (!mesh) return 0;
    switch (domain) {
        case Domain::Point: return mesh->positions.size();
        case Domain::Face: return mesh->indices.size() / 3;
        case Domain::FaceCorner: return mesh->indices.size();
        case Domain::Edge:
        case Domain::Instance: return 0;  // reserved, not stored yet
    }
    return 0;
}

std::vector<float> evaluateFloats(const FieldPtr& f, const FieldContext& ctx) {
    FieldData d;
    if (f && f->type == FieldType::Float && f->eval) f->eval(ctx, d);
    return std::move(d.floats);
}

std::vector<Vec3> evaluateVectors(const FieldPtr& f, const FieldContext& ctx) {
    FieldData d;
    if (f && f->type == FieldType::Vector3 && f->eval) f->eval(ctx, d);
    return std::move(d.vectors);
}

std::vector<uint8_t> evaluateBools(const FieldPtr& f, const FieldContext& ctx) {
    FieldData d;
    if (f && f->type == FieldType::Bool && f->eval) f->eval(ctx, d);
    return std::move(d.bools);
}

FieldPtr makeFloatField(std::function<void(const FieldContext&, std::vector<float>&)> fn) {
    auto f = std::make_shared<Field>();
    f->type = FieldType::Float;
    f->eval = [fn = std::move(fn)](const FieldContext& c, FieldData& d) { fn(c, d.floats); };
    return f;
}

FieldPtr makeVectorField(std::function<void(const FieldContext&, std::vector<Vec3>&)> fn) {
    auto f = std::make_shared<Field>();
    f->type = FieldType::Vector3;
    f->eval = [fn = std::move(fn)](const FieldContext& c, FieldData& d) { fn(c, d.vectors); };
    return f;
}

FieldPtr makeBoolField(std::function<void(const FieldContext&, std::vector<uint8_t>&)> fn) {
    auto f = std::make_shared<Field>();
    f->type = FieldType::Bool;
    f->eval = [fn = std::move(fn)](const FieldContext& c, FieldData& d) { fn(c, d.bools); };
    return f;
}

uint32_t randomHash(uint32_t contextSeed, int32_t nodeSeed, uint32_t index) {
    const uint64_t a = mix64(static_cast<uint64_t>(contextSeed) * 0x9E3779B97F4A7C15ull);
    const uint64_t b = (static_cast<uint64_t>(static_cast<uint32_t>(nodeSeed)) << 32) | index;
    return static_cast<uint32_t>(mix64(a ^ b) >> 32);
}

float randomUnit(uint32_t contextSeed, int32_t nodeSeed, uint32_t index) {
    return static_cast<float>(randomHash(contextSeed, nodeSeed, index) >> 8) * (1.0f / 16777216.0f);
}

}  // namespace Phantom::GeometryNode
