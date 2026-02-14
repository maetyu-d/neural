#include "UnitConvertNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void UnitConvertNode::setConversion(neurons::engine::core::SignalType from,
                                    neurons::engine::core::SignalType to) {
    from_ = from;
    to_ = to;
}

void UnitConvertNode::reset(double) {}

void UnitConvertNode::process(std::span<const float> inA,
                              std::span<const float>,
                              std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());

    const bool unipolarToBipolar =
        from_ == neurons::engine::core::SignalType::UnipolarAudio &&
        to_ == neurons::engine::core::SignalType::BipolarAudio;

    const bool bipolarToUnipolar =
        from_ == neurons::engine::core::SignalType::BipolarAudio &&
        to_ == neurons::engine::core::SignalType::UnipolarAudio;

    for (std::size_t i = 0; i < n; ++i) {
        if (unipolarToBipolar) {
            out[i] = (inA[i] * 2.0f) - 1.0f;
        } else if (bipolarToUnipolar) {
            out[i] = (inA[i] + 1.0f) * 0.5f;
        } else {
            out[i] = inA[i];
        }
    }
}

} // namespace neurons::engine::nodes
