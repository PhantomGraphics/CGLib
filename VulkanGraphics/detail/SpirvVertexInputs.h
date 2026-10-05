#pragma once

// Internal to CGLib/VulkanGraphics only. Minimal SPIR-V reader that reports which vertex-input
// locations a vertex shader actually consumes (its OpEntryPoint interface, Input storage class,
// Location decoration). VulkanPipeline uses it to drop vertex attributes the shader does not read,
// which keeps one shared vertex layout usable with shaders that read only a prefix of it without
// tripping the validation layer's "Vertex attribute at location N not consumed" warning.
//
// No exceptions; a malformed or unexpected module yields ok == false and callers keep every
// declared attribute (the previous behavior).

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Phantom::VKG::detail {

struct SpirvVertexInputs {
    bool ok = false;
    std::unordered_set<uint32_t> locations; // every location covered by a consumed input variable
    bool consumes(uint32_t location) const { return locations.count(location) != 0; }
};

inline SpirvVertexInputs reflectVertexInputs(const std::vector<uint32_t>& spv)
{
    constexpr uint32_t kMagic = 0x07230203u;
    constexpr uint32_t OpEntryPoint = 15, OpTypeFloat = 22, OpTypeVector = 23, OpTypeMatrix = 24,
                       OpTypeArray = 28, OpTypePointer = 32, OpConstant = 43, OpVariable = 59,
                       OpDecorate = 71;
    constexpr uint32_t DecorationLocation = 30, StorageInput = 1, ExecVertex = 0;

    SpirvVertexInputs out;
    if (spv.size() < 5 || spv[0] != kMagic) return out;

    struct Type { uint32_t op = 0; uint32_t a = 0, b = 0; };  // a/b: component, count (op specific)
    std::unordered_map<uint32_t, Type> types;                  // by result id
    std::unordered_map<uint32_t, uint32_t> constants;          // OpConstant 32-bit integer values
    std::unordered_map<uint32_t, uint32_t> pointerTo;          // pointer type id -> pointee type id
    std::unordered_map<uint32_t, uint32_t> varType;            // Input variable id -> pointer type id
    std::unordered_map<uint32_t, uint32_t> location;           // id -> Location decoration
    std::unordered_set<uint32_t> iface;                        // vertex entry point interface ids
    bool hasVertexEntry = false;

    for (size_t i = 5; i < spv.size();) {
        const uint32_t word  = spv[i];
        const uint32_t count = word >> 16;
        const uint32_t op    = word & 0xFFFFu;
        if (count == 0 || i + count > spv.size()) return out;
        const uint32_t* w = &spv[i];
        switch (op) {
        case OpEntryPoint:
            if (count >= 4 && w[1] == ExecVertex) {
                hasVertexEntry = true;
                // name is a nul-terminated string packed in words starting at w[3]
                size_t j = 3;
                bool terminated = false;
                for (; j < count && !terminated; ++j) {
                    const uint32_t v = w[j];
                    terminated = ((v & 0xFFu) == 0) || ((v >> 8 & 0xFFu) == 0) ||
                                 ((v >> 16 & 0xFFu) == 0) || ((v >> 24 & 0xFFu) == 0);
                }
                if (!terminated) return out;
                for (; j < count; ++j) iface.insert(w[j]);
            }
            break;
        case OpTypeFloat:   if (count >= 3) types[w[1]] = {op, w[2], 0}; break;           // width
        case OpTypeVector:  if (count >= 4) types[w[1]] = {op, w[2], w[3]}; break;       // component, n
        case OpTypeMatrix:  if (count >= 4) types[w[1]] = {op, w[2], w[3]}; break;       // column type, n
        case OpTypeArray:   if (count >= 4) types[w[1]] = {op, w[2], w[3]}; break;       // elem, length id
        case OpTypePointer: if (count >= 4) pointerTo[w[1]] = w[3]; break;
        case OpConstant:    if (count >= 4) constants[w[2]] = w[3]; break;               // result id, value
        case OpVariable:
            if (count >= 4 && w[3] == StorageInput) varType[w[2]] = w[1];
            break;
        case OpDecorate:
            if (count >= 4 && w[2] == DecorationLocation) location[w[1]] = w[3];
            break;
        default: break;
        }
        i += count;
    }
    if (!hasVertexEntry) return out;

    // Number of locations a type occupies (std: 64-bit 3/4-component vectors take two).
    auto locCount = [&](auto&& self, uint32_t typeId) -> uint32_t {
        auto it = types.find(typeId);
        if (it == types.end()) return 1; // scalar int/bool or unknown: one location
        const Type& t = it->second;
        switch (t.op) {
        case OpTypeFloat:  return 1;
        case OpTypeVector: {
            auto c = types.find(t.a);
            const bool wide = c != types.end() && c->second.op == OpTypeFloat && c->second.a == 64;
            return (wide && t.b > 2) ? 2u : 1u;
        }
        case OpTypeMatrix: return t.b * self(self, t.a);
        case OpTypeArray: {
            auto n = constants.find(t.b);
            return (n == constants.end() ? 1u : n->second) * self(self, t.a);
        }
        default: return 1;
        }
    };

    for (uint32_t id : iface) {
        auto v = varType.find(id);
        auto l = location.find(id);
        if (v == varType.end() || l == location.end()) continue; // outputs and built-ins
        auto p = pointerTo.find(v->second);
        const uint32_t n = p == pointerTo.end() ? 1u : locCount(locCount, p->second);
        for (uint32_t k = 0; k < n; ++k) out.locations.insert(l->second + k);
    }
    out.ok = true;
    return out;
}

} // namespace Phantom::VKG::detail
