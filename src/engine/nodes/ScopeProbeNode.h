#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class ScopeProbeNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

    float lastPeak() const;

private:
    float lastPeak_{0.0f};
};

} // namespace neurons::engine::nodes
