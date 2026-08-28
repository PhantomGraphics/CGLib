#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "CGLib/File/File/VMDFile.h"
#include "AnimationClip.h"
#include "MorphAnimator.h"
#include <map>
#include <string>

namespace Phantom::Animation {

// Converts a VMDFile (raw binary) to AnimationClip and/or MorphAnimationClip.
// VMD is 30fps; frameNo / 30.f gives time in seconds.
// Handles MMD left-hand → right-hand conversion.
class VMDConverter {
public:
    // Convert bone keyframes → AnimationClip.
    bool convert(const Phantom::File::VMDFile& vmd,
                 const std::map<std::string, int>& boneNameToIndex,
                 AnimationClip& outClip);

    // Convert morph keyframes → MorphAnimationClip.
    bool convertMorphs(const Phantom::File::VMDFile& vmd,
                       const std::map<std::string, int>& morphNameToIndex,
                       MorphAnimationClip& outClip);

private:
    static std::string sjisToUtf8(const char* sjis, int maxLen);
    static glm::vec3 convertPos(float x, float y, float z) { return {x, y, -z}; }
    // glm::quat ctor is (w,x,y,z)
    static glm::quat convertRot(float qx, float qy, float qz, float qw) {
        return {qw, -qx, -qy, qz};
    }

    // Bezier interpolation utility (VMD interpolation[64] format).
    // Control points are in [0,127]; x is normalized time in [0,1].
    static float bezierEval(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by, float x);
};

} // namespace Phantom::Animation
