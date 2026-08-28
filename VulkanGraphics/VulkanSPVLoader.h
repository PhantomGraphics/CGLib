#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Phantom::VKG {

/// @brief SPIR-V ファイル (.spv) を uint32_t ワード配列として読み込む。
///
/// VulkanPipeline / VulkanComputePipeline の config.vertSpv / config.fragSpv /
/// config.compSpv に直接渡せる形式で返す。
///
/// @param path  .spv ファイルのパス。
/// @return SPIR-V バイトコードを uint32_t ワード単位で格納したベクタ。
///         ファイルが開けない場合、またはサイズが 4 の倍数でない場合は stderr にログを出し空ベクタを返す。
///         assert は使わない（Windows の Debug CRT がダイアログを出し、自動テストが止まってしまうため）。
inline std::vector<uint32_t> loadSPV(const std::string& path)
{
    FILE* f = nullptr;
#ifdef _MSC_VER
    fopen_s(&f, path.c_str(), "rb");
#else
    f = fopen(path.c_str(), "rb");
#endif
    if (!f) {
        fprintf(stderr, "[VKG] Cannot open SPIR-V file: %s\n", path.c_str());
        return {};
    }

    fseek(f, 0, SEEK_END);
    const long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (bytes % 4 != 0) {
        fprintf(stderr, "[VKG] SPIR-V file size is not a multiple of 4: %s\n", path.c_str());
        fclose(f);
        return {};
    }

    std::vector<uint32_t> spv(static_cast<size_t>(bytes) / 4);
    fread(spv.data(), 1, static_cast<size_t>(bytes), f);
    fclose(f);
    return spv;
}

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
