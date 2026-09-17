#include "PhmatReflection.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace Phantom::Gltf::Phmat
{
namespace {

constexpr uint32_t kSpirvMagic = 0x07230203u;

// SPIR-V opcode/enum values this reflection needs (SPIR-V spec sections 3.32 "Instructions" and
// 3.7/3.20 "Storage Class"/"Decoration"). Kept as local constants instead of pulling in the full
// spirv.h the Vulkan SDK ships, since only a handful of values are needed -- see this file's
// header comment for why a third-party reflection library isn't used either.
constexpr uint32_t kOpDecorate = 71;
constexpr uint32_t kOpVariable = 59;
constexpr uint32_t kDecorationBinding = 33;
constexpr uint32_t kDecorationDescriptorSet = 34;
constexpr uint32_t kStorageClassPushConstant = 9;

} // namespace

bool reflectSpirv(const std::vector<uint32_t>& spirv, PhmatReflectionResult& outResult)
{
    outResult = PhmatReflectionResult{};
    if (spirv.size() < 5 || spirv[0] != kSpirvMagic) return false;

    std::unordered_map<uint32_t, uint32_t> bindingOf;       // decorated target id -> Binding literal
    std::unordered_map<uint32_t, uint32_t> descriptorSetOf; // decorated target id -> DescriptorSet literal
    std::vector<std::pair<uint32_t, uint32_t>> variables;   // (resultId, storageClass) for every OpVariable

    size_t i = 5; // skip the fixed 5-word header (magic, version, generator, bound, schema)
    while (i < spirv.size()) {
        const uint32_t word0 = spirv[i];
        const uint32_t wordCount = word0 >> 16;
        const uint32_t opcode = word0 & 0xFFFFu;
        if (wordCount == 0 || i + wordCount > spirv.size()) break; // malformed stream -- stop, keep what was parsed so far

        if (opcode == kOpDecorate && wordCount >= 4) {
            const uint32_t target = spirv[i + 1];
            const uint32_t decoration = spirv[i + 2];
            const uint32_t literal = spirv[i + 3];
            if (decoration == kDecorationBinding) bindingOf[target] = literal;
            else if (decoration == kDecorationDescriptorSet) descriptorSetOf[target] = literal;
        } else if (opcode == kOpVariable && wordCount >= 4) {
            // OpVariable: <id resultType> <id result> StorageClass [<id Initializer>]
            const uint32_t resultId = spirv[i + 2];
            const uint32_t storageClass = spirv[i + 3];
            variables.emplace_back(resultId, storageClass);
        }

        i += wordCount;
    }

    std::vector<PhmatDescriptorBinding> found;
    for (const auto& v : variables) {
        const uint32_t resultId = v.first;
        const uint32_t storageClass = v.second;
        if (storageClass == kStorageClassPushConstant) {
            outResult.usesPushConstants = true;
            continue;
        }
        auto setIt = descriptorSetOf.find(resultId);
        auto bindIt = bindingOf.find(resultId);
        if (setIt != descriptorSetOf.end() && bindIt != bindingOf.end()) {
            found.push_back(PhmatDescriptorBinding{ setIt->second, bindIt->second });
        }
    }

    std::sort(found.begin(), found.end(), [](const PhmatDescriptorBinding& a, const PhmatDescriptorBinding& b) {
        return a.set != b.set ? a.set < b.set : a.binding < b.binding;
    });
    found.erase(std::unique(found.begin(), found.end(), [](const PhmatDescriptorBinding& a, const PhmatDescriptorBinding& b) {
        return a.set == b.set && a.binding == b.binding;
    }), found.end());

    outResult.descriptors = std::move(found);
    return true;
}

bool isAllowedDescriptorBinding(const PhmatDescriptorBinding& b)
{
    if (b.set == 0) return b.binding <= 4; // GlobalUBO(0), irradiance/prefiltered/BRDF-LUT/shadowMap(1-4)
    if (b.set == 1) return b.binding <= 5; // MaterialUBO(0, unreferenced but valid to redeclare), 5 texture slots(1-5)
    return false;
}

bool validateReflection(const PhmatReflectionResult& result, std::vector<std::string>& outMessages)
{
    bool ok = true;
    if (result.usesPushConstants) {
        outMessages.push_back("shader declares push constants, which GltfSceneRenderer's phmat pipeline layout does not provide");
        ok = false;
    }
    for (const auto& b : result.descriptors) {
        if (!isAllowedDescriptorBinding(b)) {
            outMessages.push_back("shader declares an unsupported descriptor binding (set=" + std::to_string(b.set) +
                                   ", binding=" + std::to_string(b.binding) +
                                   ") outside GltfSceneRenderer's fixed material pipeline layout");
            ok = false;
        }
    }
    return ok;
}

}
