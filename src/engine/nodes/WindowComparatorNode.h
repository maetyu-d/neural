#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class WindowComparatorNode final : public NodeProcessor {
public:
    void setCenter(float center);
    void setWidth(float width);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float center_{0.0f};
    float width_{0.5f};
};

} // namespace neurons::engine::nodes
