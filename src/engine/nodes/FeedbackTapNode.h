#pragma once

#include "NodeProcessor.h"

#include <vector>

namespace neurons::engine::nodes {

class FeedbackTapNode final : public NodeProcessor {
public:
    void setLoopMs(float loopMs);
    void setReinject(float reinject);
    void setFreeze(bool freeze);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    void updateLoopSamples();

    double sampleRate_{48000.0};
    float loopMs_{250.0f};
    float reinject_{0.35f};
    bool freezeManual_{false};

    std::vector<float> buffer_;
    std::size_t head_{0};
    std::size_t loopSamples_{1};
};

} // namespace neurons::engine::nodes
