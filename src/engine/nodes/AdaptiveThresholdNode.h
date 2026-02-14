#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class AdaptiveThresholdNode final : public NodeProcessor {
public:
    void setBaseThreshold(float threshold);
    void setAdaptAmount(float amount);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float baseThreshold_{0.5f};
    float adaptAmount_{0.25f};
    float thresholdState_{0.5f};
    bool high_{false};
};

} // namespace neurons::engine::nodes
