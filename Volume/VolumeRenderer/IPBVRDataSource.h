#pragma once

#include "../Volume/SparseVolumeTree/SparseVolume.h"

#include <vector>

namespace Phantom::Volume {

struct PBVRSceneEntry {
    const SparseVolumef* volume; // non-owning
    bool visible;
};

class IPBVRDataSource {
public:
    virtual ~IPBVRDataSource() = default;
    virtual std::vector<PBVRSceneEntry> getPBVREntries() const = 0;
};

} // namespace PBVR
