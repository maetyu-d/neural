#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class DendriteSumNode final : public NodeProcessor {
public:
    void setGains(float gainA, float gainB);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float gainA_{1.0f};
    float gainB_{1.0f};
};

} // namespace neurons::engine::nodes
