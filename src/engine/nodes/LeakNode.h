#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class LeakNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float state_{0.0f};
    float leak_{0.02f};
};

} // namespace neurons::engine::nodes
