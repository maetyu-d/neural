#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class MidiNoteToHzNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void setQuantizeToNearestInt(bool enabled);
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    bool quantizeToNearestInt_{false};
};

} // namespace neurons::engine::nodes
