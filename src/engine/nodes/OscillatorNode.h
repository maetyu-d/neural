#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class OscillatorNode final : public NodeProcessor {
public:
    void setFrequencyHz(float hz);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float frequencyHz_{220.0f};
    float phase_{0.0f};
};

} // namespace neurons::engine::nodes
