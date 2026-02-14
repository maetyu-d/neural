#pragma once

#include "../core/SignalTypes.h"
#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class UnitConvertNode final : public NodeProcessor {
public:
    void setConversion(neurons::engine::core::SignalType from,
                       neurons::engine::core::SignalType to);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    neurons::engine::core::SignalType from_{neurons::engine::core::SignalType::BipolarAudio};
    neurons::engine::core::SignalType to_{neurons::engine::core::SignalType::UnipolarAudio};
};

} // namespace neurons::engine::nodes
