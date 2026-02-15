#include "OscillatorNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kSchmittLow = 0.3f;
constexpr float kSchmittHigh = 0.7f;
}

void OscillatorNode::setFrequencyHz(float hz) {
    frequencyHz_ = std::clamp(hz, 0.0f, 20000.0f);
}

void OscillatorNode::setWaveform(float waveformIndex) {
    const int index = std::clamp(static_cast<int>(std::lround(waveformIndex)), 0, 3);
    waveform_ = static_cast<Waveform>(index);
}

void OscillatorNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    phase_ = 0.0f;
    waveSelectHigh_ = false;
}

void OscillatorNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    processWithWaveSelect(inA, inB, {}, out);
}

void OscillatorNode::processWithWaveSelect(std::span<const float> inFreq,
                                           std::span<const float> inPhase,
                                           std::span<const float> inWaveSelect,
                                           std::span<float> out) {
    const auto n = out.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (i < inWaveSelect.size()) {
            const float selector = inWaveSelect[i];
            if (!waveSelectHigh_ && selector >= kSchmittHigh) {
                const int next = (static_cast<int>(waveform_) + 1) % 4;
                waveform_ = static_cast<Waveform>(next);
                waveSelectHigh_ = true;
            } else if (waveSelectHigh_ && selector <= kSchmittLow) {
                waveSelectHigh_ = false;
            }
        }

        const float hzCv = (i < inFreq.size()) ? inFreq[i] : 0.0f;
        const float phaseMod = (i < inPhase.size()) ? inPhase[i] : 0.0f;
        const float hz = std::clamp(frequencyHz_ + hzCv, 0.0f, 20000.0f);
        const float phaseInc = kTwoPi * hz / static_cast<float>(sampleRate_);
        out[i] = renderSample();
        phase_ += phaseInc + (phaseMod * 0.015f);
        if (phase_ >= kTwoPi) {
            phase_ -= kTwoPi;
        } else if (phase_ < 0.0f) {
            phase_ += kTwoPi;
        }
    }
}

float OscillatorNode::renderSample() const {
    const float p = phase_ / kTwoPi;
    switch (waveform_) {
    case Waveform::Sine:
        return std::sin(phase_);
    case Waveform::Triangle:
        return 1.0f - (4.0f * std::abs(p - 0.5f));
    case Waveform::Saw:
        return (2.0f * p) - 1.0f;
    case Waveform::Square:
        return p < 0.5f ? 1.0f : -1.0f;
    default:
        return std::sin(phase_);
    }
}

} // namespace neurons::engine::nodes
