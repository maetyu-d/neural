#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SlewNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float maxStep_{0.01f};
    float state_{0.0f};
};

} // namespace neurons::engine::nodes
