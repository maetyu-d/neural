#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class MixNode final : public NodeProcessor {
public:
    void setWeights(float a, float b);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float weightA_{0.5f};
    float weightB_{0.5f};
};

} // namespace neurons::engine::nodes
