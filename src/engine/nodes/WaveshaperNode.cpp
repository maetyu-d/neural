#include "WaveshaperNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void WaveshaperNode::setDrive(float drive) {
    drive_ = std::clamp(drive, 0.1f, 8.0f);
}

void WaveshaperNode::setCurve(float curve) {
    curve_ = std::clamp(curve, 0.0f, 1.0f);
}

void WaveshaperNode::reset(double) {}

void WaveshaperNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float asym = inB[i] * 0.15f;
        const float x = (inA[i] + asym) * drive_;

        const float cubic = x - (x * x * x) * 0.22f;
        const float tanhSat = std::tanh(x);
        const float y = (1.0f - curve_) * cubic + curve_ * tanhSat;

        out[i] = std::tanh(y);
    }
}

} // namespace neurons::engine::nodes
