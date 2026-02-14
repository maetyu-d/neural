#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class DriftNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    std::uint32_t state_{0x89ABCDEFu};
    float drift_{0.0f};
};

} // namespace neurons::engine::nodes
