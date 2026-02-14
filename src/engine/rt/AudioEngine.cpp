#include "AudioEngine.h"

#include "../nodes/BiquadCoreNode.h"
#include "../nodes/AddNode.h"
#include "../nodes/AdaptiveThresholdNode.h"
#include "../nodes/CompareNode.h"
#include "../nodes/ConstantNode.h"
#include "../nodes/BurstNeuronNode.h"
#include "../nodes/CrossfadeVCANode.h"
#include "../nodes/CounterNode.h"
#include "../nodes/DelayShortNode.h"
#include "../nodes/DendriteNonlinearityNode.h"
#include "../nodes/DendriteSumNode.h"
#include "../nodes/DriftNode.h"
#include "../nodes/GateNode.h"
#include "../nodes/InvertNode.h"
#include "../nodes/IntegratorNode.h"
#include "../nodes/LeakNode.h"
#include "../nodes/MixNode.h"
#include "../nodes/ModuloNode.h"
#include "../nodes/NeuronCoreNode.h"
#include "../nodes/NoiseNode.h"
#include "../nodes/OscillatorNode.h"
#include "../nodes/OscillatorPhaseNode.h"
#include "../nodes/OutputNode.h"
#include "../nodes/PhaseOpsNode.h"
#include "../nodes/PulseNode.h"
#include "../nodes/RandomGateNode.h"
#include "../nodes/RefractoryGateNode.h"
#include "../nodes/SampleHoldNode.h"
#include "../nodes/SaturatorNode.h"
#include "../nodes/SlewNode.h"
#include "../nodes/ScopeProbeNode.h"
#include "../nodes/SlopeDetectNode.h"
#include "../nodes/SpikeGeneratorNode.h"
#include "../nodes/SwitchNode.h"
#include "../nodes/SynapseNode.h"
#include "../nodes/ThresholdNode.h"
#include "../nodes/UnitConvertNode.h"
#include "../nodes/WaveshaperNode.h"
#include "../nodes/MultiplyNode.h"
#include "../nodes/MembraneLeakCapNode.h"
#include "../nodes/AllpassNode.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>

namespace neurons::engine::rt {

namespace {
constexpr float kCycleConvergenceThreshold = 1.0e-4f;
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;

bool allConverged(const std::vector<std::uint8_t>& converged, int numSamples) {
    return std::all_of(converged.begin(), converged.begin() + numSamples, [](std::uint8_t v) {
        return v != 0U;
    });
}
} // namespace

AudioEngine::AudioEngine()
    : cycleSolver_(neurons::engine::dsp::CycleConfig{}) {}

void AudioEngine::prepareToPlay(int maxBlockSize, double sampleRate) {
    {
        std::scoped_lock lock(graphMutex_);

        maxBlockSize_ = maxBlockSize;
        sampleRate_ = sampleRate;

        inputA_.resize(static_cast<std::size_t>(maxBlockSize_));
        inputB_.resize(static_cast<std::size_t>(maxBlockSize_));
        scratch_.resize(static_cast<std::size_t>(maxBlockSize_));
        dryLeft_.resize(static_cast<std::size_t>(maxBlockSize_));
        dryRight_.resize(static_cast<std::size_t>(maxBlockSize_));
        fallbackLeft_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
        fallbackRight_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
        converged_.assign(static_cast<std::size_t>(maxBlockSize_), 1U);
        iterationConverged_.assign(static_cast<std::size_t>(maxBlockSize_), 1U);

        createDefaultGraphIfEmpty();
    }

    publishGraphSnapshot();
    if (const auto snapshot = std::atomic_load(&runtimeSnapshot_); snapshot != nullptr) {
        rebuildRuntimeGraph(snapshot->graph, maxBlockSize_);
    }
}

void AudioEngine::processBlock(float* left, float* right, int numSamples) {
    if (numSamples <= 0 || left == nullptr || right == nullptr) {
        return;
    }

    if (numSamples > maxBlockSize_) {
        maxBlockSize_ = numSamples;
        inputA_.resize(static_cast<std::size_t>(maxBlockSize_));
        inputB_.resize(static_cast<std::size_t>(maxBlockSize_));
        scratch_.resize(static_cast<std::size_t>(maxBlockSize_));
        dryLeft_.resize(static_cast<std::size_t>(maxBlockSize_));
        dryRight_.resize(static_cast<std::size_t>(maxBlockSize_));
        fallbackLeft_.resize(static_cast<std::size_t>(maxBlockSize_), 0.0f);
        fallbackRight_.resize(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    }

    for (int i = 0; i < numSamples; ++i) {
        dryLeft_[static_cast<std::size_t>(i)] = left[i];
        dryRight_[static_cast<std::size_t>(i)] = right[i];
    }

    const auto snapshot = std::atomic_load(&runtimeSnapshot_);
    if (snapshot == nullptr) {
        for (int i = 0; i < numSamples; ++i) {
            const auto idx = static_cast<std::size_t>(i);
            left[i] = fallbackLeft_[idx];
            right[i] = fallbackRight_[idx];
        }
        return;
    }

    rebuildRuntimeGraph(snapshot->graph, numSamples);

    for (const auto nodeId : snapshot->schedule.acyclicOrder) {
        processNode(snapshot->graph, snapshot->nodeParams, nodeId, numSamples);
    }

    if (static_cast<int>(converged_.size()) < numSamples) {
        converged_.resize(static_cast<std::size_t>(numSamples), 1U);
        iterationConverged_.resize(static_cast<std::size_t>(numSamples), 1U);
    }
    std::fill_n(converged_.begin(), numSamples, 1U);

    if (!snapshot->schedule.cyclicNodes.empty()) {
        const auto maxIterations = cycleConfig().activeCap();

        for (std::uint8_t iter = 0; iter < maxIterations; ++iter) {
            std::fill_n(iterationConverged_.begin(), numSamples, 1U);

            for (const auto nodeId : snapshot->schedule.cyclicNodes) {
                auto outputIt = outputs_.find(nodeId);
                if (outputIt == outputs_.end()) {
                    continue;
                }

                auto previous = outputIt->second;
                processNode(snapshot->graph, snapshot->nodeParams, nodeId, numSamples);

                auto& current = outputIt->second;
                for (int sample = 0; sample < numSamples; ++sample) {
                    const auto idx = static_cast<std::size_t>(sample);
                    const auto delta = std::abs(current[idx] - previous[idx]);
                    if (delta > kCycleConvergenceThreshold) {
                        iterationConverged_[idx] = 0U;
                    }
                }
            }

            std::copy_n(iterationConverged_.begin(), numSamples, converged_.begin());
            if (allConverged(iterationConverged_, numSamples)) {
                break;
            }
        }
    }

    const auto outputNode = chooseOutputNode(snapshot->graph);
    auto outIt = outputNode.has_value() ? outputs_.find(*outputNode) : outputs_.end();

    if (outputNode.has_value()) {
        gatherInputForPort(snapshot->graph, *outputNode, 0, inputA_.data(), numSamples);
        gatherInputForPort(snapshot->graph, *outputNode, 1, inputB_.data(), numSamples);
    }

    for (int i = 0; i < numSamples; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const float rawL = outputNode.has_value() ? inputA_[idx]
                                                  : ((outIt != outputs_.end()) ? outIt->second[idx] : 0.0f);
        const float rawR = outputNode.has_value() ? inputB_[idx]
                                                  : ((outIt != outputs_.end()) ? outIt->second[idx] : 0.0f);

        float l = std::tanh(rawL);
        float r = std::tanh(rawR);

        if (converged_[idx] == 0U) {
            l = std::tanh(l * 0.5f);
            r = std::tanh(r * 0.5f);
        }

        left[i] = l;
        right[i] = r;
        fallbackLeft_[idx] = l;
        fallbackRight_[idx] = r;
    }

    const auto stats = cycleSolver_.processSamples(numSamples, converged_.data());
    (void)stats;

    // Placeholder until dedicated perf telemetry is added outside the audio callback.
    lastCpuLoadPercent_ = 0.0f;
}

void AudioEngine::releaseResources() {
    std::scoped_lock lock(graphMutex_);

    maxBlockSize_ = 0;
    sampleRate_ = 0.0;
    lastCpuLoadPercent_ = 0.0f;

    processors_.clear();
    outputs_.clear();
    switchSelectState_.clear();
    std::atomic_store(&latestScopeProbeTrace_, std::shared_ptr<const std::vector<float>>{});
    converged_.clear();
    iterationConverged_.clear();
    fallbackLeft_.clear();
    fallbackRight_.clear();
    std::atomic_store(&runtimeSnapshot_, std::shared_ptr<const RuntimeSnapshot>{});
}

const neurons::engine::dsp::CycleConfig& AudioEngine::cycleConfig() const {
    return cycleSolver_.config();
}

float AudioEngine::lastCpuLoadPercent() const {
    return lastCpuLoadPercent_;
}

float AudioEngine::latestScopeProbePeak() const {
    return latestScopeProbePeak_.load(std::memory_order_relaxed);
}

neurons::engine::core::NodeId AudioEngine::latestScopeProbeNodeId() const {
    return latestScopeProbeNodeId_.load(std::memory_order_relaxed);
}

std::vector<float> AudioEngine::latestScopeProbeTrace() const {
    const auto trace = std::atomic_load(&latestScopeProbeTrace_);
    return trace != nullptr ? *trace : std::vector<float>{};
}

neurons::engine::core::GraphModel& AudioEngine::graph() {
    return graph_;
}

std::mutex& AudioEngine::graphMutex() {
    return graphMutex_;
}

void AudioEngine::publishGraphSnapshot() {
    std::scoped_lock lock(graphMutex_);

    auto snapshot = std::make_shared<RuntimeSnapshot>();
    snapshot->graph = graph_;
    snapshot->schedule = scheduler_.buildSchedule(snapshot->graph);
    snapshot->nodeParams = nodeParams_;
    std::atomic_store(&runtimeSnapshot_, std::shared_ptr<const RuntimeSnapshot>(snapshot));
}

void AudioEngine::setNodeParam(neurons::engine::core::NodeId nodeId, const std::string& key, float value) {
    std::scoped_lock lock(graphMutex_);
    nodeParams_[nodeId][key] = value;
}

std::optional<float> AudioEngine::getNodeParam(neurons::engine::core::NodeId nodeId, const std::string& key) const {
    std::scoped_lock lock(graphMutex_);
    const auto nodeIt = nodeParams_.find(nodeId);
    if (nodeIt == nodeParams_.end()) {
        return std::nullopt;
    }
    const auto paramIt = nodeIt->second.find(key);
    if (paramIt == nodeIt->second.end()) {
        return std::nullopt;
    }
    return paramIt->second;
}

AudioEngine::NodeParamMap AudioEngine::getAllNodeParams() const {
    std::scoped_lock lock(graphMutex_);
    return nodeParams_;
}

void AudioEngine::replaceGraphAndParams(const neurons::engine::core::GraphModel& graph, const NodeParamMap& params) {
    std::scoped_lock lock(graphMutex_);
    graph_ = graph;
    nodeParams_ = params;
    processors_.clear();
    outputs_.clear();
    switchSelectState_.clear();
}

void AudioEngine::createDefaultGraphIfEmpty() {
    if (!graph_.nodes().empty()) {
        return;
    }

    using neurons::engine::core::NodeSpec;
    using neurons::engine::core::PortSpec;
    using neurons::engine::core::SignalType;

    auto makeStereoOut = []() {
        NodeSpec n;
        n.typeName = "OutputStereo";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in_l"},
            PortSpec{1, SignalType::BipolarAudio, "in_r"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "tap"},
        };
        return n;
    };
    auto makeOsc = []() {
        NodeSpec n;
        n.typeName = "Oscillator";
        n.inputs = {
            PortSpec{0, SignalType::HzAudio, "freq_hz"},
            PortSpec{1, SignalType::BipolarAudio, "phase_mod"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return n;
    };
    auto makeMix = []() {
        NodeSpec n;
        n.typeName = "Mix";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in1"},
            PortSpec{1, SignalType::BipolarAudio, "in2"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return n;
    };
    auto makeDrift = []() {
        NodeSpec n;
        n.typeName = "Drift";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "aux"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return n;
    };
    auto makeBiquad = []() {
        NodeSpec n;
        n.typeName = "BiquadCore";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::HzAudio, "cutoff"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return n;
    };
    auto makeDelay = []() {
        NodeSpec n;
        n.typeName = "DelayShort";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::TimeAudio, "time"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return n;
    };
    auto makeSat = []() {
        NodeSpec n;
        n.typeName = "Saturator";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "side"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return n;
    };
    auto makeProbe = []() {
        NodeSpec n;
        n.typeName = "ScopeProbe";
        n.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "aux"},
        };
        n.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "through"},
        };
        return n;
    };

    // Startup demo: drifting dual oscillators -> blend -> filter -> short delay -> saturator -> output.
    auto out = makeStereoOut();
    out.id = 1;
    auto oscA = makeOsc();
    oscA.id = 2;
    auto oscB = makeOsc();
    oscB.id = 3;
    auto drift = makeDrift();
    drift.id = 4;
    auto blend = makeMix();
    blend.id = 5;
    auto filter = makeBiquad();
    filter.id = 6;
    auto delay = makeDelay();
    delay.id = 7;
    auto sat = makeSat();
    sat.id = 8;
    auto probe = makeProbe();
    probe.id = 9;

    graph_.addNode(std::move(out));
    graph_.addNode(std::move(oscA));
    graph_.addNode(std::move(oscB));
    graph_.addNode(std::move(drift));
    graph_.addNode(std::move(blend));
    graph_.addNode(std::move(filter));
    graph_.addNode(std::move(delay));
    graph_.addNode(std::move(sat));
    graph_.addNode(std::move(probe));

    graph_.addConnection({4, 0, 2, 1}); // Drift modulates oscillator A phase.
    graph_.addConnection({2, 0, 5, 0});
    graph_.addConnection({3, 0, 5, 1});
    graph_.addConnection({5, 0, 6, 0});
    graph_.addConnection({6, 0, 7, 0});
    graph_.addConnection({7, 0, 8, 0});
    graph_.addConnection({8, 0, 1, 0});
    graph_.addConnection({8, 0, 1, 1});
    graph_.addConnection({8, 0, 9, 0}); // Probe final signal for diagnostics.

    nodeParams_[2]["freq_hz"] = 220.0f;
    nodeParams_[3]["freq_hz"] = 329.63f;
    nodeParams_[5]["inlets"] = 2.0f;
    nodeParams_[6]["cutoff_hz"] = 1400.0f;
    nodeParams_[7]["delay_ms"] = 1.5f;
    nodeParams_[8]["drive"] = 1.4f;
}

void AudioEngine::rebuildRuntimeGraph(const neurons::engine::core::GraphModel& graph, int numSamples) {
    for (const auto& [nodeId, spec] : graph.nodes()) {
        if (processors_.find(nodeId) == processors_.end()) {
            auto processor = createProcessor(spec.typeName);
            if (processor != nullptr) {
                processor->reset(sampleRate_);
                processors_.emplace(nodeId, std::move(processor));
            }
        }

        auto& output = outputs_[nodeId];
        if (static_cast<int>(output.size()) != numSamples) {
            output.assign(static_cast<std::size_t>(numSamples), 0.0f);
        } else {
            std::fill(output.begin(), output.end(), 0.0f);
        }
    }

    for (auto it = processors_.begin(); it != processors_.end();) {
        if (graph.nodes().find(it->first) == graph.nodes().end()) {
            outputs_.erase(it->first);
            nodeParams_.erase(it->first);
            switchSelectState_.erase(it->first);
            it = processors_.erase(it);
        } else {
            ++it;
        }
    }
}

std::unique_ptr<neurons::engine::nodes::NodeProcessor> AudioEngine::createProcessor(const std::string& typeName) const {
    using namespace neurons::engine::nodes;

    if (typeName == "Mix") {
        return std::make_unique<MixNode>();
    }
    if (typeName == "Add") {
        return std::make_unique<AddNode>();
    }
    if (typeName == "Multiply") {
        return std::make_unique<MultiplyNode>();
    }
    if (typeName == "Constant") {
        return std::make_unique<ConstantNode>();
    }
    if (typeName == "Compare") {
        return std::make_unique<CompareNode>();
    }
    if (typeName == "RandomGate") {
        return std::make_unique<RandomGateNode>();
    }
    if (typeName == "Switch") {
        return std::make_unique<SwitchNode>();
    }
    if (typeName == "SlopeDetect") {
        return std::make_unique<SlopeDetectNode>();
    }
    if (typeName == "AdaptiveThreshold") {
        return std::make_unique<AdaptiveThresholdNode>();
    }
    if (typeName == "RefractoryGate") {
        return std::make_unique<RefractoryGateNode>();
    }
    if (typeName == "SpikeGenerator") {
        return std::make_unique<SpikeGeneratorNode>();
    }
    if (typeName == "MembraneLeakCap") {
        return std::make_unique<MembraneLeakCapNode>();
    }
    if (typeName == "DendriteSum") {
        return std::make_unique<DendriteSumNode>();
    }
    if (typeName == "DendriteNonlinearity") {
        return std::make_unique<DendriteNonlinearityNode>();
    }
    if (typeName == "BurstNeuron") {
        return std::make_unique<BurstNeuronNode>();
    }
    if (typeName == "Saturator") {
        return std::make_unique<SaturatorNode>();
    }
    if (typeName == "UnitConvert") {
        return std::make_unique<UnitConvertNode>();
    }
    if (typeName == "NeuronCore") {
        return std::make_unique<NeuronCoreNode>();
    }
    if (typeName == "ScopeProbe") {
        return std::make_unique<ScopeProbeNode>();
    }
    if (typeName == "Oscillator") {
        return std::make_unique<OscillatorNode>();
    }
    if (typeName == "OutputStereo") {
        return std::make_unique<OutputNode>();
    }
    if (typeName == "Synapse") {
        return std::make_unique<SynapseNode>();
    }
    if (typeName == "Integrator") {
        return std::make_unique<IntegratorNode>();
    }
    if (typeName == "Leak") {
        return std::make_unique<LeakNode>();
    }
    if (typeName == "Threshold") {
        return std::make_unique<ThresholdNode>();
    }
    if (typeName == "Pulse") {
        return std::make_unique<PulseNode>();
    }
    if (typeName == "Gate") {
        return std::make_unique<GateNode>();
    }
    if (typeName == "Slew") {
        return std::make_unique<SlewNode>();
    }
    if (typeName == "Waveshaper") {
        return std::make_unique<WaveshaperNode>();
    }
    if (typeName == "Noise") {
        return std::make_unique<NoiseNode>();
    }
    if (typeName == "Drift") {
        return std::make_unique<DriftNode>();
    }
    if (typeName == "OscillatorPhase") {
        return std::make_unique<OscillatorPhaseNode>();
    }
    if (typeName == "PhaseOps") {
        return std::make_unique<PhaseOpsNode>();
    }
    if (typeName == "DelayShort") {
        return std::make_unique<DelayShortNode>();
    }
    if (typeName == "BiquadCore") {
        return std::make_unique<BiquadCoreNode>();
    }
    if (typeName == "SampleHold") {
        return std::make_unique<SampleHoldNode>();
    }
    if (typeName == "CrossfadeVCA") {
        return std::make_unique<CrossfadeVCANode>();
    }
    if (typeName == "Allpass") {
        return std::make_unique<AllpassNode>();
    }
    if (typeName == "Invert") {
        return std::make_unique<InvertNode>();
    }
    if (typeName == "Counter") {
        return std::make_unique<CounterNode>();
    }
    if (typeName == "Modulo") {
        return std::make_unique<ModuloNode>();
    }

    return nullptr;
}

void AudioEngine::processNode(const neurons::engine::core::GraphModel& graph,
                              const RuntimeSnapshot::ParamMap& params,
                              neurons::engine::core::NodeId nodeId,
                              int numSamples) {
    auto procIt = processors_.find(nodeId);
    if (procIt == processors_.end()) {
        return;
    }

    auto outIt = outputs_.find(nodeId);
    if (outIt == outputs_.end()) {
        return;
    }
    const auto* nodeSpec = graph.getNode(nodeId);
    if (nodeSpec == nullptr) {
        return;
    }

    const auto paramFor = [&](const char* key, float fallback) {
        const auto nodeIt = params.find(nodeId);
        if (nodeIt == params.end()) {
            return fallback;
        }
        const auto paramIt = nodeIt->second.find(key);
        if (paramIt == nodeIt->second.end()) {
            return fallback;
        }
        return paramIt->second;
    };

    if (nodeSpec->typeName == "Mix") {
        const int availableInputs = static_cast<int>(nodeSpec->inputs.size());
        const int fallbackInputs = std::clamp(availableInputs, 2, 8);
        const int requestedInputs = std::clamp(static_cast<int>(std::lround(paramFor("inlets", static_cast<float>(fallbackInputs)))), 2, 8);
        const int activeInputs = std::clamp(requestedInputs, 2, std::max(2, availableInputs));

        for (int i = 0; i < numSamples; ++i) {
            outIt->second[static_cast<std::size_t>(i)] = 0.0f;
        }

        for (int port = 0; port < activeInputs; ++port) {
            gatherInputForPort(graph, nodeId, static_cast<neurons::engine::core::PortIndex>(port), inputA_.data(), numSamples);
            for (int i = 0; i < numSamples; ++i) {
                outIt->second[static_cast<std::size_t>(i)] += inputA_[static_cast<std::size_t>(i)];
            }
        }

        const float norm = 1.0f / static_cast<float>(activeInputs);
        for (int i = 0; i < numSamples; ++i) {
            outIt->second[static_cast<std::size_t>(i)] *= norm;
        }
        return;
    }

    if (nodeSpec->typeName == "Switch") {
        gatherInputForPort(graph, nodeId, 0, inputA_.data(), numSamples);
        gatherInputForPort(graph, nodeId, 1, inputB_.data(), numSamples);
        gatherInputForPort(graph, nodeId, 2, scratch_.data(), numSamples);

        bool hasSelectConnection = false;
        for (const auto& c : graph.connections()) {
            if (c.toNode == nodeId && c.toPort == 2) {
                hasSelectConnection = true;
                break;
            }
        }

        if (!hasSelectConnection) {
            const bool selectB = (paramFor("select_b", 0.0f) >= 0.5f);
            for (int i = 0; i < numSamples; ++i) {
                outIt->second[static_cast<std::size_t>(i)] = selectB ? inputB_[static_cast<std::size_t>(i)]
                                                                     : inputA_[static_cast<std::size_t>(i)];
            }
            switchSelectState_[nodeId] = selectB;
            return;
        }

        auto stateIt = switchSelectState_.find(nodeId);
        if (stateIt == switchSelectState_.end()) {
            stateIt = switchSelectState_.emplace(nodeId, paramFor("select_b", 0.0f) >= 0.5f).first;
        }
        bool selectB = stateIt->second;
        for (int i = 0; i < numSamples; ++i) {
            const float selector = scratch_[static_cast<std::size_t>(i)];
            if (!selectB && selector >= kSchmittHigh) {
                selectB = true;
            } else if (selectB && selector <= kSchmittLow) {
                selectB = false;
            }
            outIt->second[static_cast<std::size_t>(i)] = selectB ? inputB_[static_cast<std::size_t>(i)]
                                                                 : inputA_[static_cast<std::size_t>(i)];
        }
        stateIt->second = selectB;
        return;
    }

    gatherInputForPort(graph, nodeId, 0, inputA_.data(), numSamples);
    gatherInputForPort(graph, nodeId, 1, inputB_.data(), numSamples);

    if (auto* osc = dynamic_cast<neurons::engine::nodes::OscillatorNode*>(procIt->second.get()); osc != nullptr) {
        osc->setFrequencyHz(paramFor("freq_hz", 220.0f));
    } else if (auto* biquad = dynamic_cast<neurons::engine::nodes::BiquadCoreNode*>(procIt->second.get()); biquad != nullptr) {
        biquad->setCutoffHz(paramFor("cutoff_hz", 1200.0f));
    } else if (auto* delay = dynamic_cast<neurons::engine::nodes::DelayShortNode*>(procIt->second.get()); delay != nullptr) {
        delay->setDelayMs(paramFor("delay_ms", 1.33f));
    } else if (auto* sat = dynamic_cast<neurons::engine::nodes::SaturatorNode*>(procIt->second.get()); sat != nullptr) {
        sat->setDrive(paramFor("drive", 1.0f));
    } else if (auto* shaper = dynamic_cast<neurons::engine::nodes::WaveshaperNode*>(procIt->second.get()); shaper != nullptr) {
        shaper->setDrive(paramFor("drive", 1.0f));
        shaper->setCurve(paramFor("curve", 0.5f));
    } else if (auto* allpass = dynamic_cast<neurons::engine::nodes::AllpassNode*>(procIt->second.get()); allpass != nullptr) {
        allpass->setDelayMs(paramFor("delay_ms", 6.0f));
        allpass->setFeedback(paramFor("feedback", 0.6f));
    } else if (auto* modulo = dynamic_cast<neurons::engine::nodes::ModuloNode*>(procIt->second.get()); modulo != nullptr) {
        modulo->setModulus(paramFor("modulus", 1.0f));
    } else if (auto* counter = dynamic_cast<neurons::engine::nodes::CounterNode*>(procIt->second.get()); counter != nullptr) {
        counter->setRange(paramFor("min", 0.0f), paramFor("max", 15.0f));
        counter->setWrapMode(paramFor("wrap", 1.0f) >= 0.5f);
    } else if (auto* constant = dynamic_cast<neurons::engine::nodes::ConstantNode*>(procIt->second.get()); constant != nullptr) {
        constant->setValue(paramFor("value", 0.0f));
    } else if (auto* compare = dynamic_cast<neurons::engine::nodes::CompareNode*>(procIt->second.get()); compare != nullptr) {
        compare->setGreaterMode(paramFor("greater", 1.0f) >= 0.5f);
    } else if (auto* randomGate = dynamic_cast<neurons::engine::nodes::RandomGateNode*>(procIt->second.get()); randomGate != nullptr) {
        randomGate->setProbability(paramFor("prob", 0.5f));
        randomGate->setPulseMs(paramFor("pulse_ms", 2.0f));
    } else if (auto* probe = dynamic_cast<neurons::engine::nodes::ScopeProbeNode*>(procIt->second.get()); probe != nullptr) {
        (void)probe;
    } else if (auto* slope = dynamic_cast<neurons::engine::nodes::SlopeDetectNode*>(procIt->second.get()); slope != nullptr) {
        slope->setThreshold(paramFor("threshold", 1.0e-4f));
    } else if (auto* adaptive = dynamic_cast<neurons::engine::nodes::AdaptiveThresholdNode*>(procIt->second.get()); adaptive != nullptr) {
        adaptive->setBaseThreshold(paramFor("base_threshold", 0.5f));
        adaptive->setAdaptAmount(paramFor("adapt", 0.25f));
    } else if (auto* refractory = dynamic_cast<neurons::engine::nodes::RefractoryGateNode*>(procIt->second.get()); refractory != nullptr) {
        refractory->setRefractoryMs(paramFor("refractory_ms", 30.0f));
        refractory->setPulseMs(paramFor("pulse_ms", 1.0f));
    } else if (auto* spike = dynamic_cast<neurons::engine::nodes::SpikeGeneratorNode*>(procIt->second.get()); spike != nullptr) {
        spike->setThreshold(paramFor("threshold", 0.5f));
        spike->setPulseMs(paramFor("pulse_ms", 1.0f));
    } else if (auto* membrane = dynamic_cast<neurons::engine::nodes::MembraneLeakCapNode*>(procIt->second.get()); membrane != nullptr) {
        membrane->setTauMs(paramFor("tau_ms", 20.0f));
        membrane->setLeak(paramFor("leak", 0.01f));
    } else if (auto* dendriteSum = dynamic_cast<neurons::engine::nodes::DendriteSumNode*>(procIt->second.get()); dendriteSum != nullptr) {
        dendriteSum->setGains(paramFor("gain_a", 1.0f), paramFor("gain_b", 1.0f));
    } else if (auto* dendriteNl = dynamic_cast<neurons::engine::nodes::DendriteNonlinearityNode*>(procIt->second.get()); dendriteNl != nullptr) {
        dendriteNl->setDrive(paramFor("drive", 1.0f));
        dendriteNl->setBias(paramFor("bias", 0.0f));
    } else if (auto* burst = dynamic_cast<neurons::engine::nodes::BurstNeuronNode*>(procIt->second.get()); burst != nullptr) {
        burst->setCount(paramFor("count", 3.0f));
        burst->setIntervalMs(paramFor("interval_ms", 8.0f));
    } else if (auto* neuron = dynamic_cast<neurons::engine::nodes::NeuronCoreNode*>(procIt->second.get()); neuron != nullptr) {
        neurons::engine::nodes::NeuronCoreNode::Params p;
        p.gain = paramFor("gain", 1.0f);
        p.tauMs = paramFor("tau_ms", 20.0f);
        neuron->setParams(p);
    }

    procIt->second->process(std::span<const float>(inputA_.data(), static_cast<std::size_t>(numSamples)),
                            std::span<const float>(inputB_.data(), static_cast<std::size_t>(numSamples)),
                            std::span<float>(outIt->second.data(), static_cast<std::size_t>(numSamples)));

    if (auto* probe = dynamic_cast<neurons::engine::nodes::ScopeProbeNode*>(procIt->second.get()); probe != nullptr) {
        latestScopeProbePeak_.store(probe->lastPeak(), std::memory_order_relaxed);
        latestScopeProbeNodeId_.store(nodeId, std::memory_order_relaxed);
        const auto& source = outIt->second;
        constexpr std::size_t kTracePoints = 256;
        const std::size_t srcSize = source.size();
        const std::size_t points = std::min<std::size_t>(kTracePoints, srcSize);
        auto trace = std::make_shared<std::vector<float>>();
        trace->resize(points);
        if (points > 0 && srcSize > 0) {
            for (std::size_t i = 0; i < points; ++i) {
                const std::size_t idx = (i * srcSize) / points;
                (*trace)[i] = source[std::min(idx, srcSize - 1)];
            }
        }
        std::atomic_store(&latestScopeProbeTrace_, std::shared_ptr<const std::vector<float>>(trace));
    }
}

void AudioEngine::gatherInputForPort(const neurons::engine::core::GraphModel& graph,
                                     neurons::engine::core::NodeId nodeId,
                                     neurons::engine::core::PortIndex port,
                                     float* out,
                                     int numSamples) const {
    if (out == nullptr || numSamples <= 0) {
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        out[i] = 0.0f;
    }

    for (const auto& c : graph.connections()) {
        if (c.toNode != nodeId || c.toPort != port) {
            continue;
        }

        const auto srcIt = outputs_.find(c.fromNode);
        if (srcIt == outputs_.end()) {
            continue;
        }

        const auto& source = srcIt->second;
        for (int i = 0; i < numSamples; ++i) {
            out[i] += source[static_cast<std::size_t>(i)];
        }
    }
}

std::optional<neurons::engine::core::NodeId> AudioEngine::chooseOutputNode(const neurons::engine::core::GraphModel& graph) const {
    for (const auto& [nodeId, spec] : graph.nodes()) {
        if (spec.typeName == "OutputStereo") {
            return {nodeId};
        }
    }
    return std::nullopt;
}

} // namespace neurons::engine::rt
