#pragma once

#include "ParticleSet.h"
#include "TransferFunction.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"

#include <random>

namespace Phantom::Volume {

class ParticleGenerator {
public:
    void setTransferFunction(const TransferFunction* tf) { tf_ = tf; }
    void setDensityScale(float s) { densityScale_ = s; }

    ParticleSet generate(const Phantom::Volume::SparseVolumef& sv) const;

private:
    const TransferFunction* tf_ = nullptr;
    float densityScale_ = 1.0f;
    mutable std::mt19937 rng_{42};
};

} // namespace PBVR
