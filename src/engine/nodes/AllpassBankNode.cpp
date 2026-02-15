#include "AllpassBankNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void AllpassBankNode::setBaseDelayMs(float delayMs) {
    baseDelayMs_ = std::clamp(delayMs, 0.1f, 30.0f);
    updateDelays();
}

void AllpassBankNode::setFeedback(float feedback) {
    feedback_ = std::clamp(feedback, -0.99f, 0.99f);
}

void AllpassBankNode::setSpread(float spread) {
    spread_ = std::clamp(spread, 0.0f, 0.95f);
    updateDelays();
}

void AllpassBankNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    for (std::size_t i = 0; i < kStages; ++i) {
        buffers_[i].assign(4096, 0.0f);
        writes_[i] = 0;
        delays_[i] = 1;
    }
    updateDelays();
}

void AllpassBankNode::process(std::span<const float> inA,
                              std::span<const float> inB,
                              std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float mod = std::clamp(inB[i], -1.0f, 1.0f);
        const float dynFeedback = std::clamp(feedback_ + (0.15f * mod), -0.99f, 0.99f);
        float x = inA[i];
        for (std::size_t s = 0; s < kStages; ++s) {
            auto& buffer = buffers_[s];
            const float stageScale = 1.0f + spread_ * static_cast<float>(s);
            const float dynDelayMs = std::clamp(baseDelayMs_ * stageScale * (1.0f + 0.4f * mod), 0.1f, 40.0f);
            const auto dynSamples = static_cast<std::size_t>((dynDelayMs * 0.001f) * static_cast<float>(sampleRate_));
            const auto tap = std::clamp<std::size_t>(dynSamples, 1, 4095);
            const auto read = (writes_[s] + buffer.size() - tap) % buffer.size();
            const float delayed = buffer[read];
            const float y = -dynFeedback * x + delayed;
            buffer[writes_[s]] = x + dynFeedback * y;
            writes_[s] = (writes_[s] + 1) % buffer.size();
            x = y;
        }
        out[i] = x;
    }
}

void AllpassBankNode::updateDelays() {
    for (std::size_t s = 0; s < kStages; ++s) {
        const float scale = 1.0f + spread_ * static_cast<float>(s);
        const auto samples = static_cast<std::size_t>((baseDelayMs_ * scale * 0.001f) * static_cast<float>(sampleRate_));
        delays_[s] = std::clamp<std::size_t>(samples, 1, 4095);
    }
}

} // namespace neurons::engine::nodes
