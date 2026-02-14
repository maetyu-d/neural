#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class CompareNode final : public NodeProcessor {
public:
    void setGreaterMode(bool greaterMode);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    bool greaterMode_{true};
};

} // namespace neurons::engine::nodes
