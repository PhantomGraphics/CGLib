#pragma once
#include <cmath>
#include <optional>
namespace Phantom::Math::Noise {
enum class FractalMode { None, FBm, Turbulence, Ridged, RidgedMultifractal };
struct FractalSettings { FractalMode mode=FractalMode::FBm; unsigned octaves=5; float lacunarity=2.0f; float gain=0.5f; float ridgeOffset=1.0f; float ridgeFeedback=2.0f; };
inline bool validateFractalSettings(const FractalSettings& s) {
    if (s.octaves<1 || s.octaves>12 || !std::isfinite(s.lacunarity) || s.lacunarity<=1 || !std::isfinite(s.gain) || s.gain<0 || s.gain>1 || unsigned(s.mode)>4) return false;
    return s.mode != FractalMode::RidgedMultifractal || (std::isfinite(s.ridgeOffset) && s.ridgeOffset>0 && std::isfinite(s.ridgeFeedback) && s.ridgeFeedback>=0);
}
template<class Sample> std::optional<float> evaluateFractal(Sample sample, const FractalSettings& s={}) {
    if (!validateFractalSettings(s)) return std::nullopt;
    float frequency=1, amplitude=1, sum=0, weights=0, weight=1;
    for(unsigned i=0;i<(s.mode==FractalMode::None?1:s.octaves);++i) {
        if (!std::isfinite(frequency)) return std::nullopt;
        auto value=sample(frequency); if(!value || !std::isfinite(*value)) return std::nullopt;
        float n=*value;
        if(s.mode==FractalMode::Turbulence) n=std::abs(n);
        if(s.mode==FractalMode::Ridged) { n=std::fmax(0.0f,1-std::abs(n)); n*=n; }
        if(s.mode==FractalMode::RidgedMultifractal) {
            n=std::fmax(0.0f,s.ridgeOffset-std::abs(n)); n=n*n*weight;
            if(!std::isfinite(n)) return std::nullopt;
            const float feedback=n*s.ridgeFeedback;
            if(!std::isfinite(feedback)) return std::nullopt;
            weight=std::fmin(1.0f,feedback);
        }
        sum+=amplitude*n; weights+=amplitude; amplitude*=s.gain; frequency*=s.lacunarity;
    }
    if (!(weights > 0.0f) || !std::isfinite(sum)) return std::nullopt;
    return sum/weights;
}
}
