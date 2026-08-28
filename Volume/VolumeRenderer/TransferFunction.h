#pragma once

#include <vector>

namespace Phantom::Volume {

struct TFSample {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

class TransferFunction {
public:
    void setPoint(float scalar, float r, float g, float b, float a);
    TFSample sample(float scalar) const;
    const std::vector<TFSample>& getLUT() const { return lut_; }
    void buildLUT(int resolution = 256);

private:
    struct ControlPoint {
        float s = 0.0f;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    };

    std::vector<ControlPoint> points_;
    std::vector<TFSample> lut_;
};

} // namespace Phantom::Volume
