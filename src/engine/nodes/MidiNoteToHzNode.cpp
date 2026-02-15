#include "MidiNoteToHzNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void MidiNoteToHzNode::reset(double) {}

void MidiNoteToHzNode::setQuantizeToNearestInt(bool enabled) {
    quantizeToNearestInt_ = enabled;
}

void MidiNoteToHzNode::process(std::span<const float> inA,
                               std::span<const float> inB,
                               std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        float note = inA[i] + inB[i];
        if (quantizeToNearestInt_) {
            note = std::round(note);
        }
        const float hz = 440.0f * std::exp2((note - 69.0f) / 12.0f);
        out[i] = std::clamp(hz, 0.0f, 20000.0f);
    }
}

} // namespace neurons::engine::nodes
