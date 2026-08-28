#include "Skeleton.h"

namespace Phantom::Animation {

std::vector<int> Skeleton::rootBoneIndices() const {
    std::vector<int> roots;
    for (int i = 0; i < static_cast<int>(bones.size()); ++i) {
        if (bones[i].parentIndex == -1)
            roots.push_back(i);
    }
    return roots;
}

int Skeleton::addBone(Bone bone) {
    const int idx = static_cast<int>(bones.size());
    boneNameToIndex[bone.name] = idx;
    bones.push_back(std::move(bone));
    return idx;
}

} // namespace Phantom::Animation
