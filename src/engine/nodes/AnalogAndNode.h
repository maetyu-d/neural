#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class AnalogAndNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;
};

} // namespace neurons::engine::nodes
