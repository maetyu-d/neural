#pragma once

#include "NodeProcessor.h"

#include <vector>

namespace neurons::engine::nodes {

class CombFilterNode final : public NodeProcessor {
public:
    void setDelayMs(float delayMs);
    void setFeedback(float feedback);
    void setDamping(float damping);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    void updateDelay();

    double sampleRate_{48000.0};
    float delayMs_{18.0f};
    float feedback_{0.75f};
    float damping_{0.2f};
    float lpState_{0.0f};
    std::vector<float> buffer_;
    std::size_t write_{0};
    std::size_t delaySamples_{1};
};

} // namespace neurons::engine::nodes
