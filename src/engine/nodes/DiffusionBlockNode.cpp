#include "DiffusionBlockNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void DiffusionBlockNode::setSizeMs(float sizeMs) {
    sizeMs_ = std::clamp(sizeMs, 0.5f, 60.0f);
    updateDelays();
}

void DiffusionBlockNode::setFeedback(float feedback) {
    feedback_ = std::clamp(feedback, -0.99f, 0.99f);
}

void DiffusionBlockNode::setMix(float mix) {
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void DiffusionBlockNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    delayA_.assign(8192, 0.0f);
    delayB_.assign(8192, 0.0f);
    writeA_ = 0;
    writeB_ = 0;
    updateDelays();
}

void DiffusionBlockNode::process(std::span<const float> inA,
                                 std::span<const float> inB,
                                 std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float mod = std::clamp(inB[i], -1.0f, 1.0f);
        const float sizeScale = std::clamp(1.0f + 0.5f * mod, 0.5f, 1.5f);
        const auto dynA = static_cast<std::size_t>((sizeMs_ * sizeScale * 0.001f) * static_cast<float>(sampleRate_));
        const auto dynB = static_cast<std::size_t>(((sizeMs_ * 1.618f * sizeScale) * 0.001f) * static_cast<float>(sampleRate_));
        const auto tapA = std::clamp<std::size_t>(dynA, 1, 8191);
        const auto tapB = std::clamp<std::size_t>(dynB, 1, 8191);
        const float dynFeedback = std::clamp(feedback_ + 0.2f * mod, -0.99f, 0.99f);
        const float dynMix = std::clamp(mix_ + 0.25f * mod, 0.0f, 1.0f);

        const auto readA = (writeA_ + delayA_.size() - tapA) % delayA_.size();
        const auto readB = (writeB_ + delayB_.size() - tapB) % delayB_.size();

        const float a = delayA_[readA];
        const float b = delayB_[readB];
        const float x = inA[i];

        const float sum = 0.70710678f * (a + b);
        const float diff = 0.70710678f * (a - b);

        delayA_[writeA_] = x + dynFeedback * diff;
        delayB_[writeB_] = x + dynFeedback * sum;

        const float wet = 0.6f * (sum + diff);
        out[i] = (1.0f - dynMix) * x + dynMix * wet;

        writeA_ = (writeA_ + 1) % delayA_.size();
        writeB_ = (writeB_ + 1) % delayB_.size();
    }
}

void DiffusionBlockNode::updateDelays() {
    const auto a = static_cast<std::size_t>((sizeMs_ * 0.001f) * static_cast<float>(sampleRate_));
    const auto b = static_cast<std::size_t>(((sizeMs_ * 1.618f) * 0.001f) * static_cast<float>(sampleRate_));
    delaySamplesA_ = std::clamp<std::size_t>(a, 1, 8191);
    delaySamplesB_ = std::clamp<std::size_t>(b, 1, 8191);
}

} // namespace neurons::engine::nodes
