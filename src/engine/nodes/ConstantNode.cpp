#include "ConstantNode.h"

namespace neurons::engine::nodes {

void ConstantNode::setValue(float value) {
    value_ = value;
}

void ConstantNode::reset(double) {}

void ConstantNode::process(std::span<const float>,
                           std::span<const float>,
                           std::span<float> out) {
    for (float& sample : out) {
        sample = value_;
    }
}

} // namespace neurons::engine::nodes
