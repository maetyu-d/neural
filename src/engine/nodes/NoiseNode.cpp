#include "NoiseNode.h"

namespace neurons::engine::nodes {

void NoiseNode::reset(double) {
    state_ = 0x12345678u;
}

void NoiseNode::process(std::span<const float>,
                        std::span<const float>,
                        std::span<float> out) {
    for (std::size_t i = 0; i < out.size(); ++i) {
        state_ = state_ * 1664525u + 1013904223u;
        const float n = static_cast<float>((state_ >> 8) & 0x00FFFFFFu) / 8388608.0f - 1.0f;
        out[i] = n * 0.2f;
    }
}

} // namespace neurons::engine::nodes
