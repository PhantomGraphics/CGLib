#include "VulkanDebugUtils.h"

#include <unordered_map>

namespace Phantom::VKG::DebugUtils {
namespace {

struct FunctionTable {
    PFN_vkCmdBeginDebugUtilsLabelEXT  cmdBeginLabel  = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT    cmdEndLabel    = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT cmdInsertLabel = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT  setObjectName  = nullptr;
};

const FunctionTable& tableFor(VkInstance instance)
{
    // One VkInstance per process in every app this codebase ships (see this file's header
    // comment) -- a small map keyed by the handle is simply the least-ceremony way to resolve
    // once and cache, without adding a global "the one instance" singleton.
    static std::unordered_map<VkInstance, FunctionTable> cache;
    auto it = cache.find(instance);
    if (it != cache.end()) return it->second;

    FunctionTable table;
    table.cmdBeginLabel  = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    table.cmdEndLabel    = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
    table.cmdInsertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
    table.setObjectName  = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
    return cache.emplace(instance, table).first->second;
}

VkDebugUtilsLabelEXT makeLabel(const char* name, float r, float g, float b, float a)
{
    VkDebugUtilsLabelEXT label{};
    label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = r; label.color[1] = g; label.color[2] = b; label.color[3] = a;
    return label;
}

} // namespace

void beginLabel(VkInstance instance, VkCommandBuffer cmd, const char* name,
                 float r, float g, float b, float a)
{
    const FunctionTable& fn = tableFor(instance);
    if (!fn.cmdBeginLabel) return;
    VkDebugUtilsLabelEXT label = makeLabel(name, r, g, b, a);
    fn.cmdBeginLabel(cmd, &label);
}

void endLabel(VkInstance instance, VkCommandBuffer cmd)
{
    const FunctionTable& fn = tableFor(instance);
    if (!fn.cmdEndLabel) return;
    fn.cmdEndLabel(cmd);
}

void insertLabel(VkInstance instance, VkCommandBuffer cmd, const char* name,
                  float r, float g, float b, float a)
{
    const FunctionTable& fn = tableFor(instance);
    if (!fn.cmdInsertLabel) return;
    VkDebugUtilsLabelEXT label = makeLabel(name, r, g, b, a);
    fn.cmdInsertLabel(cmd, &label);
}

void setObjectName(VkInstance instance, VkDevice device, VkObjectType objectType,
                    uint64_t objectHandle, const char* name)
{
    const FunctionTable& fn = tableFor(instance);
    if (!fn.setObjectName) return;
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType   = objectType;
    info.objectHandle = objectHandle;
    info.pObjectName  = name;
    fn.setObjectName(device, &info);
}

} // namespace Phantom::VKG::DebugUtils
