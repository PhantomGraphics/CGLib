#pragma once

#include "Bone.h"

#include <map>
#include <string>
#include <vector>

namespace Phantom::Animation {

struct Skeleton {
    std::vector<Bone>         bones;
    std::map<std::string,int> boneNameToIndex;

    // Returns indices of all root bones (parentIndex == -1).
    std::vector<int> rootBoneIndices() const;

    // Adds a bone and registers its name. Returns the new bone index.
    int addBone(Bone bone);
};

} // namespace Phantom::Animation
