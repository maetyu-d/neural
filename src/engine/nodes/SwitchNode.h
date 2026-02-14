#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SwitchNode final : public NodeProcessor {
public:
    void setSelectB(bool selectB);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    bool selectB_{false};
};

} // namespace neurons::engine::nodes
