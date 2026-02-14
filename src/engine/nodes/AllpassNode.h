#pragma once

#include "NodeProcessor.h"

#include <vector>

namespace neurons::engine::nodes {

class AllpassNode final : public NodeProcessor {
public:
    void setDelayMs(float delayMs);
    void setFeedback(float feedback);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    void updateDelaySamples();

    double sampleRate_{48000.0};
    float delayMs_{6.0f};
    float feedback_{0.6f};
    std::vector<float> buffer_;
    std::size_t write_{0};
    std::size_t delaySamples_{1};
};

} // namespace neurons::engine::nodes
