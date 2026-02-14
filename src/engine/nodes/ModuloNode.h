#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class ModuloNode final : public NodeProcessor {
public:
    void setModulus(float modulus);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float modulus_{1.0f};
};

} // namespace neurons::engine::nodes
