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
                                 std::span<const float>,
                                 std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto readA = (writeA_ + delayA_.size() - delaySamplesA_) % delayA_.size();
        const auto readB = (writeB_ + delayB_.size() - delaySamplesB_) % delayB_.size();

        const float a = delayA_[readA];
        const float b = delayB_[readB];
        const float x = inA[i];

        const float sum = 0.70710678f * (a + b);
        const float diff = 0.70710678f * (a - b);

        delayA_[writeA_] = x + feedback_ * diff;
        delayB_[writeB_] = x + feedback_ * sum;

        const float wet = 0.6f * (sum + diff);
        out[i] = (1.0f - mix_) * x + mix_ * wet;

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
