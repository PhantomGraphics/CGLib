#pragma once
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <optional>
namespace Phantom::Math::Noise {
namespace detail {
class SplitMix32 {
public:
    explicit SplitMix32(uint32_t seed) : state_(seed) {}

    uint32_t next() {
        state_ += 0x9E3779B9u;
        uint32_t z = state_;
        z = (z ^ (z >> 16)) * 0x21F0AAADu;
        z = (z ^ (z >> 15)) * 0x735A2D97u;
        return z ^ (z >> 15);
    }

private:
    uint32_t state_;
};

} // namespace detail

// --- Seeded 2D gradient noise -------------------------------------------------
// Classic Perlin-style value in roughly [-1, 1]. The seed selects the
// permutation via a Fisher-Yates shuffle; a plain coordinate offset is
// deliberately avoided (plan section 3.1).
class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed) {
        std::array<uint8_t, 256> p{};
        for (int i = 0; i < 256; ++i) {
            p[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
        }
        detail::SplitMix32 rng(seed);
        for (int i = 255; i > 0; --i) {
            const uint32_t j = rng.next() % static_cast<uint32_t>(i + 1);
            const uint8_t tmp = p[static_cast<size_t>(i)];
            p[static_cast<size_t>(i)] = p[j];
            p[j] = tmp;
        }
        for (size_t i = 0; i < 256; ++i) {
            perm_[i] = perm_[i + 256] = p[i];
        }
    }

private:
    float sample(float x, float y) const {
        const float fx = std::floor(x);
        const float fy = std::floor(y);
        const int xi = static_cast<int>(fx) & 255;
        const int yi = static_cast<int>(fy) & 255;
        const float xf = x - fx;
        const float yf = y - fy;

        const float u = fade(xf);
        const float v = fade(yf);

        const int aa = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi)] + yi)];
        const int ab = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi)] + yi + 1)];
        const int ba = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi + 1)] + yi)];
        const int bb = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi + 1)] + yi + 1)];

        const float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1.0f, yf), u);
        const float x2 = lerp(grad(ab, xf, yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);
        return lerp(x1, x2, v);
    }

public:
    static bool validCoordinate(float x) { return std::isfinite(x) && double(x) >= -2147483646.0 && double(x) <= 2147483645.0; }
    std::optional<float> sample2D(float x, float y) const {
        if (!validCoordinate(x) || !validCoordinate(y)) return std::nullopt;
        return sample(x,y);
    }
    std::optional<float> sample3D(float x, float y, float z) const {
        if (!validCoordinate(x) || !validCoordinate(y) || !validCoordinate(z)) return std::nullopt;
        int X=int(std::floor(x))&255, Y=int(std::floor(y))&255, Z=int(std::floor(z))&255;
        x-=std::floor(x); y-=std::floor(y); z-=std::floor(z);
        auto g=[](int h,float x,float y,float z) { h &= 15; float u=h<8?x:y, v=h<4?y:((h==12||h==14)?x:z); return ((h&1)?-u:u)+((h&2)?-v:v); };
        auto at=[&](int i,int j,int k) { return perm_[perm_[perm_[i]+j]+k]; };
        float u=fade(x),v=fade(y),w=fade(z);
        float planes[2];
        for(int k=0;k<2;++k) planes[k]=lerp(lerp(g(at(X,Y,Z+k),x,y,z-k),g(at(X+1,Y,Z+k),x-1,y,z-k),u),lerp(g(at(X,Y+1,Z+k),x,y-1,z-k),g(at(X+1,Y+1,Z+k),x-1,y-1,z-k),u),v);
        return lerp(planes[0],planes[1],w);
    }
private:
    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }
    static float grad(int hash, float x, float y) {
        switch (hash & 7) {
            case 0: return x + y;
            case 1: return -x + y;
            case 2: return x - y;
            case 3: return -x - y;
            case 4: return x;
            case 5: return -x;
            case 6: return y;
            default: return -y;
        }
    }

    std::array<uint8_t, 512> perm_{};
};

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
