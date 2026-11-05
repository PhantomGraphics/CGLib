#pragma once
#include "PerlinNoise.h"
#include "../Core/Vector3d.h"
namespace Phantom::Math::Noise {
struct DomainWarpSettings {
    float strength=1, scale=1;
    Vector3df offset{0};
    unsigned octaves=3;
    float lacunarity=2, gain=.5f;
};
inline bool validateDomainWarpSettings(const DomainWarpSettings& s) {
    return std::isfinite(s.strength) && s.strength>=0 && std::isfinite(s.scale) && s.scale>0 &&
        std::isfinite(s.offset.x) && std::isfinite(s.offset.y) && std::isfinite(s.offset.z) &&
        validateFractalSettings({FractalMode::FBm,s.octaves,s.lacunarity,s.gain});
}
class DomainWarp {
public:
    explicit DomainWarp(uint32_t seed) : DomainWarp(detail::SplitMix32(seed)) {}
    std::optional<Vector3df> sample3D(Vector3df p, const DomainWarpSettings& s={}) const { return sample(p,s,true); }
    // 2D operates on XZ and preserves Y.
    std::optional<Vector3df> sample2D(Vector3df p, const DomainWarpSettings& s={}) const { return sample(p,s,false); }
private:
    explicit DomainWarp(detail::SplitMix32 rng) : x_(rng.next()), y_(rng.next()), z_(rng.next()) {}
    std::optional<Vector3df> sample(Vector3df p,const DomainWarpSettings& s,bool three) const {
        if(!validateDomainWarpSettings(s) || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return std::nullopt;
        if(s.strength==0) return p;
        const Vector3df q=p*s.scale+s.offset;
        auto component=[&](const PerlinNoise& noise) {
            return evaluateFractal([&](float f) { return three ? noise.sample3D(q.x*f,q.y*f,q.z*f) : noise.sample2D(q.x*f,q.z*f); }, {FractalMode::FBm,s.octaves,s.lacunarity,s.gain});
        };
        const auto x=component(x_), z=component(three ? z_ : y_);
        const auto y=three ? component(y_) : std::optional<float>(0.0f);
        if(!x || !y || !z) return std::nullopt;
        Vector3df result=p+s.strength*Vector3df(*x,*y,*z);
        if(!three) result.y=p.y;
        if(!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) return std::nullopt;
        return result;
    }
    PerlinNoise x_,y_,z_;
};
}

