#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>

namespace Phantom::Animation {

struct Bone {
    std::string name;
    int         parentIndex = -1;          // -1 = root bone
    glm::mat4   bindPoseInverse{1.f};      // inverse bind-pose matrix for skinning
    glm::vec3   localPosition{0.f, 0.f, 0.f};
    glm::quat   localRotation{1.f, 0.f, 0.f, 0.f};
    glm::vec3   localScale{1.f, 1.f, 1.f};
};

} // namespace Phantom::Animation
