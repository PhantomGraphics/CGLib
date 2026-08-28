#include "VMDConverter.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Phantom::Animation {

#ifdef _WIN32
std::string VMDConverter::sjisToUtf8(const char* sjis, int maxLen)
{
    int len = 0;
    while (len < maxLen && sjis[len] != '\0') ++len;
    if (len == 0) return {};

    int wLen = MultiByteToWideChar(932, 0, sjis, len, nullptr, 0);
    if (wLen <= 0) return {};
    std::wstring wstr(wLen, L'\0');
    MultiByteToWideChar(932, 0, sjis, len, &wstr[0], wLen);

    int uLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (uLen <= 1) return {};
    std::string utf8(uLen - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], uLen, nullptr, nullptr);
    return utf8;
}
#else
// No <windows.h> on Linux -- glibc's iconv (always available, no extra link
// dependency) replaces MultiByteToWideChar/WideCharToMultiByte(CP932). Same
// codepage semantics as the Windows path above, just via a different API.
std::string VMDConverter::sjisToUtf8(const char* sjis, int maxLen)
{
    int len = 0;
    while (len < maxLen && sjis[len] != '\0') ++len;
    if (len == 0) return {};

    iconv_t cd = iconv_open("UTF-8", "CP932");
    if (cd == reinterpret_cast<iconv_t>(-1)) return {};

    std::vector<char> in(sjis, sjis + len);
    std::vector<char> out(static_cast<size_t>(len) * 4 + 4, '\0');
    char* inPtr = in.data();
    size_t inBytesLeft = in.size();
    char* outPtr = out.data();
    size_t outBytesLeft = out.size();

    size_t result = iconv(cd, &inPtr, &inBytesLeft, &outPtr, &outBytesLeft);
    iconv_close(cd);
    if (result == static_cast<size_t>(-1)) return {};

    return std::string(out.data(), out.size() - outBytesLeft);
}
#endif

float VMDConverter::bezierEval(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by, float x)
{
    float ax01 = ax / 127.f, ay01 = ay / 127.f;
    float bx01 = bx / 127.f, by01 = by / 127.f;

    float t = x;
    for (int iter = 0; iter < 10; ++iter) {
        float ft  = 3.f*(1.f-t)*(1.f-t)*t*ax01 + 3.f*(1.f-t)*t*t*bx01 + t*t*t - x;
        float dft = 3.f*(1.f-t)*(1.f-2.f*t)*ax01 + 3.f*t*(2.f-3.f*t)*bx01 + 3.f*t*t;
        if (std::abs(dft) < 1e-6f) break;
        t -= ft / dft;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
    }
    return 3.f*(1.f-t)*(1.f-t)*t*ay01 + 3.f*(1.f-t)*t*t*by01 + t*t*t;
}

bool VMDConverter::convert(const Phantom::File::VMDFile& vmd,
                            const std::map<std::string, int>& boneNameToIndex,
                            AnimationClip& outClip)
{
    outClip = AnimationClip{};
    outClip.ticksPerSecond = 30.f;

    if (vmd.boneKeyframes.empty()) return true;

    // Group keyframes by bone name
    std::unordered_map<std::string,
        std::vector<const Phantom::File::VMDBoneKeyframe*>> groups;
    for (const auto& kf : vmd.boneKeyframes) {
        std::string name = sjisToUtf8(kf.boneName, 15);
        if (name.empty()) continue;
        groups[name].push_back(&kf);
    }

    uint32_t maxFrame = 0;

    for (auto& [boneName, frames] : groups) {
        auto it = boneNameToIndex.find(boneName);
        if (it == boneNameToIndex.end()) continue;

        std::sort(frames.begin(), frames.end(),
                  [](const auto* a, const auto* b){ return a->frameNo < b->frameNo; });

        BoneChannel ch;
        ch.boneIndex = it->second;

        for (const auto* kf : frames) {
            if (kf->frameNo > maxFrame) maxFrame = kf->frameNo;
            float t = kf->frameNo / 30.f;

            ch.positionKeys.push_back({t, convertPos(kf->position[0],
                                                       kf->position[1],
                                                       kf->position[2])});
            ch.rotationKeys.push_back({t, convertRot(kf->rotation[0],
                                                       kf->rotation[1],
                                                       kf->rotation[2],
                                                       kf->rotation[3])});
            ch.scaleKeys.push_back({t, glm::vec3{1.f}});
        }

        outClip.channels.push_back(std::move(ch));
    }

    outClip.duration = maxFrame > 0 ? maxFrame / 30.f : 0.f;
    return true;
}

bool VMDConverter::convertMorphs(const Phantom::File::VMDFile& vmd,
                                   const std::map<std::string, int>& morphNameToIndex,
                                   MorphAnimationClip& outClip)
{
    outClip = MorphAnimationClip{};
    if (vmd.morphKeyframes.empty()) return true;

    std::unordered_map<std::string,
        std::vector<const Phantom::File::VMDMorphKeyframe*>> groups;
    for (const auto& kf : vmd.morphKeyframes) {
        std::string name = sjisToUtf8(kf.morphName, 15);
        if (name.empty()) continue;
        groups[name].push_back(&kf);
    }

    uint32_t maxFrame = 0;
    for (auto& [morphName, frames] : groups) {
        auto it = morphNameToIndex.find(morphName);
        if (it == morphNameToIndex.end()) continue;

        std::sort(frames.begin(), frames.end(),
            [](const auto* a, const auto* b){ return a->frameNo < b->frameNo; });

        MorphChannel ch;
        ch.morphIndex = it->second;
        for (const auto* kf : frames) {
            if (kf->frameNo > maxFrame) maxFrame = kf->frameNo;
            ch.keyframes.push_back({kf->frameNo / 30.f, kf->weight});
        }
        outClip.channels.push_back(std::move(ch));
    }

    outClip.duration = maxFrame > 0 ? maxFrame / 30.f : 0.f;
    return true;
}

} // namespace Phantom::Animation
