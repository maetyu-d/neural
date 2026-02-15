#include "ADEnvelopeNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;
}

void ADEnvelopeNode::setAttackMs(float ms) {
    attackMs_ = std::max(ms, 0.02f);
}

void ADEnvelopeNode::setDecayMs(float ms) {
    decayMs_ = std::max(ms, 0.02f);
}

void ADEnvelopeNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    level_ = 0.0f;
    inputHigh_ = false;
    stage_ = Stage::Idle;
}

void ADEnvelopeNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float trig = inA[i];
        if (!inputHigh_ && trig >= kSchmittHigh) {
            inputHigh_ = true;
            stage_ = Stage::Attack;
        } else if (inputHigh_ && trig <= kSchmittLow) {
            inputHigh_ = false;
        }

        const float timeScale = std::clamp(1.0f + inB[i], 0.1f, 4.0f);
        const auto attackSamples = std::max<std::uint32_t>(
            1, static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(attackMs_ * timeScale) * 0.001));
        const auto decaySamples = std::max<std::uint32_t>(
            1, static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(decayMs_ * timeScale) * 0.001));

        switch (stage_) {
        case Stage::Attack:
            level_ += 1.0f / static_cast<float>(attackSamples);
            if (level_ >= 1.0f) {
                level_ = 1.0f;
                stage_ = Stage::Decay;
            }
            break;
        case Stage::Decay:
            level_ -= 1.0f / static_cast<float>(decaySamples);
            if (level_ <= 0.0f) {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
        case Stage::Idle:
        default:
            break;
        }

        out[i] = level_;
    }
}

} // namespace neurons::engine::nodes
