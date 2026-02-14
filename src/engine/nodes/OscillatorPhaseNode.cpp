#include "OscillatorPhaseNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
}

void OscillatorPhaseNode::reset(double) {
    phase_ = 0.0f;
}

void OscillatorPhaseNode::process(std::span<const float> inA,
                                  std::span<const float> inB,
                                  std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        phase_ += 0.003f + (inA[i] * 0.0005f) + (inB[i] * 0.0002f);
        while (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }
        while (phase_ < 0.0f) {
            phase_ += 1.0f;
        }
        out[i] = std::sin(phase_ * kTwoPi);
    }
}

} // namespace neurons::engine::nodes
