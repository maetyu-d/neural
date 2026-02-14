#include "OscillatorNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
}

void OscillatorNode::setFrequencyHz(float hz) {
    frequencyHz_ = std::clamp(hz, 1.0f, 20000.0f);
}

void OscillatorNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    phase_ = 0.0f;
}

void OscillatorNode::process(std::span<const float>,
                             std::span<const float>,
                             std::span<float> out) {
    const float phaseInc = kTwoPi * frequencyHz_ / static_cast<float>(sampleRate_);

    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = std::sin(phase_);
        phase_ += phaseInc;
        if (phase_ >= kTwoPi) {
            phase_ -= kTwoPi;
        }
    }
}

} // namespace neurons::engine::nodes
