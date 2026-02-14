#include "NeuronCoreNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void NeuronCoreNode::setParams(const Params& params) {
    params_ = params;
}

void NeuronCoreNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    state_ = 0.0f;
    thresholdLatched_ = false;
}

void NeuronCoreNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});

    const auto tauSamples = std::max(1.0f, params_.tauMs * 0.001f * static_cast<float>(sampleRate_));
    const auto alpha = std::clamp(2.0f / (tauSamples + 1.0f), 0.00001f, 1.0f);

    for (std::size_t i = 0; i < n; ++i) {
        const float combinedIn = (params_.gain * inA[i]) + (0.5f * inB[i]) + params_.bias;

        if (!thresholdLatched_ && combinedIn >= (params_.threshold + params_.hysteresis)) {
            thresholdLatched_ = true;
        } else if (thresholdLatched_ && combinedIn <= (params_.threshold - params_.hysteresis)) {
            thresholdLatched_ = false;
        }

        const float gateBoost = thresholdLatched_ ? 0.2f : 0.0f;
        const float nonlinear = std::tanh((combinedIn + gateBoost) * params_.satDrive);
        const float target = nonlinear - (params_.leak * state_);

        state_ += alpha * (target - state_);
        out[i] = std::tanh(state_ * params_.satDrive);
    }
}

} // namespace neurons::engine::nodes
