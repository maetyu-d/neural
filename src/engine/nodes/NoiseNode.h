#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class NoiseNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    std::uint32_t state_{0x12345678u};
};

} // namespace neurons::engine::nodes
