#include "SwitchNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SwitchNode::setSelectB(bool selectB) {
    selectB_ = selectB;
}

void SwitchNode::reset(double) {}

void SwitchNode::process(std::span<const float> inA,
                         std::span<const float> inB,
                         std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = selectB_ ? inB[i] : inA[i];
    }
}

} // namespace neurons::engine::nodes
