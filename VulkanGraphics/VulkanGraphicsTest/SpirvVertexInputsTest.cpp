#include <gtest/gtest.h>

#include "../detail/SpirvVertexInputs.h"

#include <cstdint>
#include <vector>

using Phantom::VKG::detail::reflectVertexInputs;

namespace {

// Tiny SPIR-V assembler: header + instructions.
struct Spv {
    std::vector<uint32_t> w{0x07230203u, 0x00010000u, 0u, 100u, 0u};
    void op(uint32_t opcode, std::vector<uint32_t> operands) {
        w.push_back(((uint32_t(operands.size()) + 1u) << 16) | opcode);
        w.insert(w.end(), operands.begin(), operands.end());
    }
};

constexpr uint32_t OpEntryPoint = 15, OpTypeFloat = 22, OpTypeVector = 23, OpTypeMatrix = 24,
                   OpTypePointer = 32, OpVariable = 59, OpDecorate = 71;
constexpr uint32_t kLocation = 30, kInput = 1, kOutput = 3;

// "main" packed as a nul-terminated SPIR-V string: 'm','a','i','n' then a zero word.
const std::vector<uint32_t> kMainName = {0x6E69616Du, 0u};

// Vertex shader with inputs: loc 0 vec3 (used), loc 1 vec2 (declared, NOT in the interface),
// loc 2 mat4 (used, occupies 2..5), plus an output at location 0.
Spv makeShader() {
    Spv s;
    // ids: 1 float, 2 vec3, 3 vec2, 4 vec4, 5 mat4, 6..8 ptr(Input) vec3/vec2/mat4, 9 ptr(Output) vec3
    // vars: 10 in vec3 loc0, 11 in vec2 loc1, 12 in mat4 loc2, 13 out vec3 loc0
    std::vector<uint32_t> entry = {0u /*Vertex*/, 99u};
    entry.insert(entry.end(), kMainName.begin(), kMainName.end());
    entry.insert(entry.end(), {10u, 12u, 13u});                     // interface: not 11
    s.op(OpEntryPoint, entry);
    s.op(OpDecorate, {10u, kLocation, 0u});
    s.op(OpDecorate, {11u, kLocation, 1u});
    s.op(OpDecorate, {12u, kLocation, 2u});
    s.op(OpDecorate, {13u, kLocation, 0u});
    s.op(OpTypeFloat, {1u, 32u});
    s.op(OpTypeVector, {2u, 1u, 3u});
    s.op(OpTypeVector, {3u, 1u, 2u});
    s.op(OpTypeVector, {4u, 1u, 4u});
    s.op(OpTypeMatrix, {5u, 4u, 4u});
    s.op(OpTypePointer, {6u, kInput, 2u});
    s.op(OpTypePointer, {7u, kInput, 3u});
    s.op(OpTypePointer, {8u, kInput, 5u});
    s.op(OpTypePointer, {9u, kOutput, 2u});
    s.op(OpVariable, {6u, 10u, kInput});
    s.op(OpVariable, {7u, 11u, kInput});
    s.op(OpVariable, {8u, 12u, kInput});
    s.op(OpVariable, {9u, 13u, kOutput});
    return s;
}

} // namespace

TEST(SpirvVertexInputsTest, ReportsOnlyInterfaceInputLocations) {
    const auto r = reflectVertexInputs(makeShader().w);
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.consumes(0));   // vec3 input in the interface
    EXPECT_FALSE(r.consumes(1));  // declared but not in the entry point interface
    for (uint32_t loc = 2; loc <= 5; ++loc) EXPECT_TRUE(r.consumes(loc)) << loc; // mat4 = 4 locations
    EXPECT_FALSE(r.consumes(6));
}

TEST(SpirvVertexInputsTest, RejectsMalformedModules) {
    EXPECT_FALSE(reflectVertexInputs({}).ok);
    EXPECT_FALSE(reflectVertexInputs({1u, 2u, 3u, 4u, 5u}).ok);   // bad magic
    Spv truncated = makeShader();
    truncated.w.resize(truncated.w.size() - 2);                   // last instruction cut short
    EXPECT_FALSE(reflectVertexInputs(truncated.w).ok);
    Spv noEntry;                                                  // valid header, no vertex entry point
    EXPECT_FALSE(reflectVertexInputs(noEntry.w).ok);
}
