#pragma once

#include "NodeProcessor.h"

#include <vector>

namespace neurons::engine::nodes {

class DelayShortNode final : public NodeProcessor {
public:
    void setDelayMs(float delayMs);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float delayMs_{1.33f};
    std::vector<float> buffer_;
    std::size_t write_{0};
    std::size_t delaySamples_{64};
};

} // namespace neurons::engine::nodes
