#include "WindowComparatorNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void WindowComparatorNode::setCenter(float center) {
    center_ = std::clamp(center, -1.0f, 1.0f);
}

void WindowComparatorNode::setWidth(float width) {
    width_ = std::clamp(width, 0.0f, 2.0f);
}

void WindowComparatorNode::reset(double) {}

void WindowComparatorNode::process(std::span<const float> inA,
                                   std::span<const float> inB,
                                   std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float c = center_ + inB[i];
        const float half = 0.5f * width_;
        out[i] = (inA[i] >= (c - half) && inA[i] <= (c + half)) ? 1.0f : 0.0f;
    }
}

} // namespace neurons::engine::nodes
