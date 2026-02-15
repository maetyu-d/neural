#include "BiquadCoreNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void BiquadCoreNode::setCutoffHz(float cutoffHz) {
    cutoffHz_ = std::clamp(cutoffHz, 20.0f, 18000.0f);
}

void BiquadCoreNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    z1_ = 0.0f;
    z2_ = 0.0f;
}

void BiquadCoreNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    const float q = 0.707f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    for (std::size_t i = 0; i < n; ++i) {
        const float cutoff = std::clamp(cutoffHz_ + inB[i], 20.0f, 18000.0f);
        const float w0 = kTwoPi * cutoff / static_cast<float>(sampleRate_);
        const float alpha = std::sin(w0) / (2.0f * q);

        const float b0 = (1.0f - std::cos(w0)) * 0.5f;
        const float b1 = 1.0f - std::cos(w0);
        const float b2 = (1.0f - std::cos(w0)) * 0.5f;
        const float a0 = 1.0f + alpha;
        const float a1 = -2.0f * std::cos(w0);
        const float a2 = 1.0f - alpha;

        const float nb0 = b0 / a0;
        const float nb1 = b1 / a0;
        const float nb2 = b2 / a0;
        const float na1 = a1 / a0;
        const float na2 = a2 / a0;

        const float x = inA[i];
        const float y = nb0 * x + z1_;
        z1_ = nb1 * x - na1 * y + z2_;
        z2_ = nb2 * x - na2 * y;
        out[i] = y;
    }
}

} // namespace neurons::engine::nodes
