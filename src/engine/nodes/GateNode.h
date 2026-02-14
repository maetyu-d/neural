#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class GateNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float threshold_{0.5f};
};

} // namespace neurons::engine::nodes
