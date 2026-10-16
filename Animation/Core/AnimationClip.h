#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace Phantom::Animation {

struct Vec3Key { float time; glm::vec3 value; };
struct QuatKey { float time; glm::quat value; };

struct BoneChannel {
    int boneIndex = -1;
    std::vector<Vec3Key> positionKeys;
    std::vector<QuatKey> rotationKeys;
    std::vector<Vec3Key> scaleKeys;
};

struct AnimationClip {
    std::string              name;
    float                    duration       = 0.f;  // seconds
    float                    ticksPerSecond = 30.f; // VMD default: 30 fps
    std::vector<BoneChannel> channels;
};

} // namespace Phantom::Animation
