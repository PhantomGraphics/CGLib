#pragma once

#include <vulkan/vulkan.h>
#include <cstdio>

// Internal to CGLib/VulkanGraphics only. Logs to stderr and returns failRet
// instead of throwing - VulkanGraphics itself must not use exceptions (see
// the project's coding conventions). CGLib/VulkanGraphics/VkCheck.h (the throwing
// VK_CHECK macro) has been removed; CGLib/GltfRenderer/IBL/GltfIBLPrecomputer.cpp
// was its last caller and now defines its own non-throwing GLTF_IBL_CHECK instead.
#define VKG_CHECK(expr, msg, failRet) \
    do { \
        if ((expr) != VK_SUCCESS) { \
            std::fprintf(stderr, "[VKG] %s\n", (msg)); \
            return (failRet); \
        } \
    } while (false)
