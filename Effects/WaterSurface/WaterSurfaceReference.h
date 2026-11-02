#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Phantom::Effects::WaterSurface {
struct WaterSurfaceConfig {
    uint32_t columns = 256, rows = 256;
    float width = 10, depth = 10, waveSpeed = 1, damping = 0.5f;
};
struct WaterImpulse { float x = 0, z = 0, radius = 0.25f, velocity = 1; };
struct WaterCell { float height = 0, velocity = 0; };
static_assert(sizeof(WaterCell) == 8);

inline bool isValidWaterSurfaceConfig(const WaterSurfaceConfig& c) {
    if(c.columns<3 || c.rows<3 || c.columns>4096 || c.rows>4096 ||
       !std::isfinite(c.width) || !std::isfinite(c.depth) ||
       !std::isfinite(c.waveSpeed) || !std::isfinite(c.damping) ||
       c.width<=0 || c.depth<=0 || c.waveSpeed<0 || c.damping<0) return false;
    const float dx=c.width/(c.columns-1), dz=c.depth/(c.rows-1);
    // Reject spacings/speeds whose shader arithmetic cannot be represented.
    return dx>0 && dz>0 && std::isfinite(1/(dx*dx)) && std::isfinite(1/(dz*dz)) &&
           std::isfinite(dx*dx) && std::isfinite(dz*dz) && std::isfinite(c.waveSpeed*c.waveSpeed);
}
inline double waterSurfaceStepSize(const WaterSurfaceConfig& c) {
    const double dx=double(c.width)/(c.columns-1), dz=double(c.depth)/(c.rows-1);
    return c.waveSpeed==0 ? 1.0/120 :
        std::min(1.0/120,0.9/(c.waveSpeed*std::sqrt(1/(dx*dx)+1/(dz*dz))));
}

// CPU oracle for the GPU implementation. Coordinates span [0,width] x [0,depth].
class WaterSurfaceReference {
public:
    bool create(const WaterSurfaceConfig& c) {
        if (!isValidWaterSurfaceConfig(c)) return false;
        config_ = c;
        cells_.assign(size_t(c.columns)*c.rows, {});
        next_ = cells_;
        accumulator_ = 0;
        return true;
    }
    void reset() { std::fill(cells_.begin(), cells_.end(), WaterCell{}); accumulator_ = 0; }
    bool addImpulse(const WaterImpulse& p) {
        if (cells_.empty() || !std::isfinite(p.x) || !std::isfinite(p.z) ||
            !std::isfinite(p.radius) || !std::isfinite(p.velocity) || p.radius <= 0) return false;
        for (uint32_t y=1; y+1<config_.rows; ++y)
            for (uint32_t x=1; x+1<config_.columns; ++x) {
                const float dx=x*(config_.width/(config_.columns-1))-p.x;
                const float dz=y*(config_.depth/(config_.rows-1))-p.z;
                const float q=std::sqrt(dx*dx+dz*dz)/p.radius;
                if(q<1) cells_[size_t(y)*config_.columns+x].velocity +=
                    p.velocity * (1-q*q)*(1-q*q);
            }
        return true;
    }
    double stepSize() const {
        return waterSurfaceStepSize(config_);
    }
    void advance(double elapsed, bool paused=false) {
        if(cells_.empty() || paused || !std::isfinite(elapsed) || elapsed<=0) return;
        const double dt=stepSize();
        accumulator_=std::min(accumulator_+elapsed,16*dt);
        for(int i=0; i<16 && accumulator_+dt*1e-9>=dt; ++i) {
            singleStep(); accumulator_=std::max(0.0,accumulator_-dt);
        }
    }
    void singleStep() {
        if(cells_.empty()) return;
        const float dt=static_cast<float>(stepSize());
        const float dx=config_.width/(config_.columns-1), dz=config_.depth/(config_.rows-1);
        std::fill(next_.begin(),next_.end(),WaterCell{});
        for(uint32_t y=1;y+1<config_.rows;++y) for(uint32_t x=1;x+1<config_.columns;++x) {
            const size_t i=size_t(y)*config_.columns+x;
            const float h=cells_[i].height;
            const float lap=(cells_[i-1].height-2*h+cells_[i+1].height)/(dx*dx)+
                (cells_[i-config_.columns].height-2*h+cells_[i+config_.columns].height)/(dz*dz);
            const float v=(cells_[i].velocity+config_.waveSpeed*config_.waveSpeed*lap*dt)*std::exp(-config_.damping*dt);
            next_[i]={h+v*dt,v};
        }
        cells_.swap(next_);
    }
    const std::vector<WaterCell>& cells() const { return cells_; }
private:
    WaterSurfaceConfig config_;
    std::vector<WaterCell> cells_, next_;
    double accumulator_ = 0;
};
}
