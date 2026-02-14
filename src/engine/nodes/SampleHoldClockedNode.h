#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SampleHoldClockedNode final : public NodeProcessor {
public:
    void setLowHigh(float low, float high);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float low_{0.3f};
    float high_{0.7f};
    bool armed_{true};
    float held_{0.0f};
};

} // namespace neurons::engine::nodes
