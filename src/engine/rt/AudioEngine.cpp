#include "AudioEngine.h"

#include "../nodes/BiquadCoreNode.h"
#include "../nodes/AddNode.h"
#include "../nodes/AdaptiveThresholdNode.h"
#include "../nodes/AnalogAndNode.h"
#include "../nodes/AnalogNandNode.h"
#include "../nodes/AnalogNorNode.h"
#include "../nodes/AnalogOrNode.h"
#include "../nodes/AnalogXorNode.h"
#include "../nodes/CompareNode.h"
#include "../nodes/ConstantNode.h"
#include "../nodes/BurstNeuronNode.h"
#include "../nodes/BytebeatJsNode.h"
#include "../nodes/CrossfadeVCANode.h"
#include "../nodes/CounterNode.h"
#include "../nodes/DelayShortNode.h"
#include "../nodes/DendriteNonlinearityNode.h"
#include "../nodes/DendriteSumNode.h"
#include "../nodes/DriftNode.h"
#include "../nodes/FeedbackTapNode.h"
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
#include "../nodes/SchmittTriggerNode.h"
#include "../nodes/SlopeDetectNode.h"
#include "../nodes/SpikeGeneratorNode.h"
#include "../nodes/SwitchNode.h"
#include "../nodes/SynapseNode.h"
#include "../nodes/ThresholdNode.h"
#include "../nodes/UnitConvertNode.h"
#include "../nodes/WindowComparatorNode.h"
#include "../nodes/WaveshaperNode.h"
#include "../nodes/MultiplyNode.h"
#include "../nodes/MembraneLeakCapNode.h"
#include "../nodes/AllpassNode.h"
#include "../nodes/AllpassBankNode.h"
#include "../nodes/CombFilterNode.h"
#include "../nodes/DiffusionBlockNode.h"
#include "../nodes/SampleHoldClockedNode.h"
#include "../nodes/SampleHoldGatedNode.h"
#include "../nodes/SampleHoldQuantizedNode.h"
#include "../nodes/SampleHoldSlewNode.h"
#include "../nodes/SamplePlayerWavNode.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <chrono>
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
    const auto blockStart = std::chrono::high_resolution_clock::now();

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
                    for (std::size_t port = 0; port < current.size() && port < previous.size(); ++port) {
                        const auto delta =
                            std::abs(current[port][idx] - previous[port][idx]);
                        if (delta > kCycleConvergenceThreshold) {
                            iterationConverged_[idx] = 0U;
                            break;
                        }
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
                                                  : ((outIt != outputs_.end() && !outIt->second.empty())
                                                         ? outIt->second[0][idx]
                                                         : 0.0f);
        const float rawR = outputNode.has_value() ? inputB_[idx]
                                                  : ((outIt != outputs_.end() && !outIt->second.empty())
                                                         ? outIt->second[0][idx]
                                                         : 0.0f);

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

    const auto blockEnd = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(blockEnd - blockStart).count();
    const double blockDurationSec =
        (sampleRate_ > 0.0) ? (static_cast<double>(numSamples) / sampleRate_) : 0.0;
    const float instantCpu =
        blockDurationSec > 0.0 ? static_cast<float>(100.0 * (elapsedSec / blockDurationSec)) : 0.0f;
    const float prev = lastCpuLoadPercent_.load(std::memory_order_relaxed);
    const float smoothed = prev + 0.2f * (instantCpu - prev);
    lastCpuLoadPercent_.store(std::clamp(smoothed, 0.0f, 999.0f), std::memory_order_relaxed);
}

void AudioEngine::releaseResources() {
    std::scoped_lock lock(graphMutex_);

    maxBlockSize_ = 0;
    sampleRate_ = 0.0;
    lastCpuLoadPercent_.store(0.0f, std::memory_order_relaxed);

    processors_.clear();
    outputs_.clear();
    switchSelectState_.clear();
    nodeScripts_.clear();
    sampleClips_.clear();
    std::atomic_store(&latestScopeProbeTrace_, std::shared_ptr<const std::vector<float>>{});
    std::atomic_store(&observedNodeTrace_, std::shared_ptr<const std::vector<float>>{});
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
    return lastCpuLoadPercent_.load(std::memory_order_relaxed);
}

bool AudioEngine::loadWavFileForNode(neurons::engine::core::NodeId nodeId, const std::string& path) {
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::File file(path);
    if (!file.existsAsFile() || !file.hasFileExtension(".wav")) {
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0) {
        return false;
    }

    const auto samplesCount = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> temp(static_cast<int>(reader->numChannels), samplesCount);
    reader->read(&temp, 0, samplesCount, 0, true, true);

    std::vector<float> mono(static_cast<std::size_t>(samplesCount), 0.0f);
    for (int i = 0; i < samplesCount; ++i) {
        float sum = 0.0f;
        for (unsigned int ch = 0; ch < reader->numChannels; ++ch) {
            sum += temp.getSample(static_cast<int>(ch), i);
        }
        mono[static_cast<std::size_t>(i)] = sum / static_cast<float>(std::max(1u, reader->numChannels));
    }

    std::scoped_lock lock(graphMutex_);
    sampleClips_[nodeId] = SampleClip{std::move(mono), reader->sampleRate, file.getFileName().toStdString()};
    if (auto it = processors_.find(nodeId); it != processors_.end()) {
        if (auto* player = dynamic_cast<neurons::engine::nodes::SamplePlayerWavNode*>(it->second.get()); player != nullptr) {
            const auto& clip = sampleClips_[nodeId];
            player->setClip(clip.samples, clip.sourceRate, clip.name);
        }
    }
    return true;
}

std::string AudioEngine::sampleClipNameForNode(neurons::engine::core::NodeId nodeId) const {
    std::scoped_lock lock(graphMutex_);
    if (const auto it = sampleClips_.find(nodeId); it != sampleClips_.end()) {
        return it->second.name;
    }
    return {};
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

void AudioEngine::setObservedNode(std::optional<neurons::engine::core::NodeId> nodeId) {
    observedNodeId_.store(nodeId.value_or(0), std::memory_order_relaxed);
}

std::optional<neurons::engine::core::NodeId> AudioEngine::observedNodeId() const {
    const auto id = observedNodeId_.load(std::memory_order_relaxed);
    if (id == 0) {
        return std::nullopt;
    }
    return id;
}

std::vector<float> AudioEngine::observedNodeTrace() const {
    const auto trace = std::atomic_load(&observedNodeTrace_);
    return trace != nullptr ? *trace : std::vector<float>{};
}

float AudioEngine::observedNodePeak() const {
    return observedNodePeak_.load(std::memory_order_relaxed);
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

void AudioEngine::setNodeScript(neurons::engine::core::NodeId nodeId, const std::string& key, const std::string& value) {
    std::scoped_lock lock(graphMutex_);
    nodeScripts_[nodeId][key] = value;
    if (auto it = processors_.find(nodeId); it != processors_.end()) {
        if (auto* bb = dynamic_cast<neurons::engine::nodes::BytebeatJsNode*>(it->second.get()); bb != nullptr) {
            if (key == "expr") {
                bb->setExpression(value);
            }
        }
    }
}

std::string AudioEngine::getNodeScript(neurons::engine::core::NodeId nodeId, const std::string& key) const {
    std::scoped_lock lock(graphMutex_);
    const auto it = nodeScripts_.find(nodeId);
    if (it == nodeScripts_.end()) {
        return {};
    }
    const auto it2 = it->second.find(key);
    if (it2 == it->second.end()) {
        return {};
    }
    return it2->second;
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
    for (auto it = nodeScripts_.begin(); it != nodeScripts_.end();) {
        if (graph_.getNode(it->first) == nullptr) {
            it = nodeScripts_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = sampleClips_.begin(); it != sampleClips_.end();) {
        if (graph_.getNode(it->first) == nullptr) {
            it = sampleClips_.erase(it);
        } else {
            ++it;
        }
    }
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
                if (auto* player = dynamic_cast<neurons::engine::nodes::SamplePlayerWavNode*>(processor.get()); player != nullptr) {
                    if (const auto clipIt = sampleClips_.find(nodeId); clipIt != sampleClips_.end()) {
                        player->setClip(clipIt->second.samples, clipIt->second.sourceRate, clipIt->second.name);
                    }
                }
                if (auto* bb = dynamic_cast<neurons::engine::nodes::BytebeatJsNode*>(processor.get()); bb != nullptr) {
                    if (const auto scriptIt = nodeScripts_.find(nodeId); scriptIt != nodeScripts_.end()) {
                        if (const auto exprIt = scriptIt->second.find("expr"); exprIt != scriptIt->second.end()) {
                            bb->setExpression(exprIt->second);
                        }
                    }
                }
                processors_.emplace(nodeId, std::move(processor));
            }
        }

        const std::size_t portCount = std::max<std::size_t>(1, spec.outputs.size());
        auto& outputs = outputs_[nodeId];
        if (outputs.size() != portCount) {
            outputs.assign(portCount, std::vector<float>(static_cast<std::size_t>(numSamples), 0.0f));
        }
        for (auto& portBuffer : outputs) {
            if (static_cast<int>(portBuffer.size()) != numSamples) {
                portBuffer.assign(static_cast<std::size_t>(numSamples), 0.0f);
            } else {
                std::fill(portBuffer.begin(), portBuffer.end(), 0.0f);
            }
        }
    }

    for (auto it = processors_.begin(); it != processors_.end();) {
        if (graph.nodes().find(it->first) == graph.nodes().end()) {
            outputs_.erase(it->first);
            nodeParams_.erase(it->first);
            nodeScripts_.erase(it->first);
            sampleClips_.erase(it->first);
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
    if (typeName == "AnalogAnd") {
        return std::make_unique<AnalogAndNode>();
    }
    if (typeName == "AnalogOr") {
        return std::make_unique<AnalogOrNode>();
    }
    if (typeName == "AnalogXor") {
        return std::make_unique<AnalogXorNode>();
    }
    if (typeName == "AnalogNand") {
        return std::make_unique<AnalogNandNode>();
    }
    if (typeName == "AnalogNor") {
        return std::make_unique<AnalogNorNode>();
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
    if (typeName == "SchmittTrigger") {
        return std::make_unique<SchmittTriggerNode>();
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
    if (typeName == "BytebeatJs") {
        return std::make_unique<BytebeatJsNode>();
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
    if (typeName == "SampleHoldGated") {
        return std::make_unique<SampleHoldGatedNode>();
    }
    if (typeName == "SampleHoldClocked") {
        return std::make_unique<SampleHoldClockedNode>();
    }
    if (typeName == "SampleHoldSlew") {
        return std::make_unique<SampleHoldSlewNode>();
    }
    if (typeName == "SampleHoldQuantized") {
        return std::make_unique<SampleHoldQuantizedNode>();
    }
    if (typeName == "SamplePlayerWav") {
        return std::make_unique<SamplePlayerWavNode>();
    }
    if (typeName == "CrossfadeVCA") {
        return std::make_unique<CrossfadeVCANode>();
    }
    if (typeName == "Allpass") {
        return std::make_unique<AllpassNode>();
    }
    if (typeName == "AllpassBank") {
        return std::make_unique<AllpassBankNode>();
    }
    if (typeName == "CombFilter") {
        return std::make_unique<CombFilterNode>();
    }
    if (typeName == "DiffusionBlock") {
        return std::make_unique<DiffusionBlockNode>();
    }
    if (typeName == "FeedbackTap") {
        return std::make_unique<FeedbackTapNode>();
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
    if (typeName == "WindowComparator") {
        return std::make_unique<WindowComparatorNode>();
    }

    return nullptr;
}

void AudioEngine::processNode(const neurons::engine::core::GraphModel& graph,
                              const RuntimeSnapshot::ParamMap& params,
                              neurons::engine::core::NodeId nodeId,
                              int numSamples) {
    auto outIt = outputs_.find(nodeId);
    if (outIt == outputs_.end()) {
        return;
    }
    auto& nodeOutputs = outIt->second;
    if (nodeOutputs.empty()) {
        return;
    }
    const auto* nodeSpec = graph.getNode(nodeId);
    if (nodeSpec == nullptr) {
        return;
    }
    auto procIt = processors_.find(nodeId);

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

    if (nodeSpec->typeName == "MatrixMixer") {
        const int inCount = std::min<int>(4, static_cast<int>(nodeSpec->inputs.size()));
        const int outCount = std::min<int>(4, static_cast<int>(nodeSpec->outputs.size()));
        const float globalGain = paramFor("global_gain", 1.0f);
        const float globalPan = std::clamp(paramFor("global_pan", 0.0f), -1.0f, 1.0f);
        for (int outPort = 0; outPort < outCount; ++outPort) {
            auto& out = nodeOutputs[static_cast<std::size_t>(outPort)];
            std::fill_n(out.begin(), numSamples, 0.0f);
        }

        for (int inPort = 0; inPort < inCount; ++inPort) {
            gatherInputForPort(graph, nodeId, static_cast<neurons::engine::core::PortIndex>(inPort), inputA_.data(), numSamples);
            for (int outPort = 0; outPort < outCount; ++outPort) {
                const std::string gainKey = "g_" + std::to_string(inPort) + "_" + std::to_string(outPort);
                const std::string panKey = "p_" + std::to_string(inPort) + "_" + std::to_string(outPort);
                const float gain = paramFor(gainKey.c_str(), inPort == outPort ? 1.0f : 0.0f) * globalGain;
                const float pan = std::clamp(paramFor(panKey.c_str(), 0.0f) + globalPan, -1.0f, 1.0f);
                const float keep = 0.5f * (1.0f - pan);
                const float spread = 0.5f * (1.0f + pan);
                auto& outA = nodeOutputs[static_cast<std::size_t>(outPort)];
                auto& outB = nodeOutputs[static_cast<std::size_t>((outPort + 1) % outCount)];
                for (int i = 0; i < numSamples; ++i) {
                    const float x = inputA_[static_cast<std::size_t>(i)] * gain;
                    outA[static_cast<std::size_t>(i)] += x * keep;
                    outB[static_cast<std::size_t>(i)] += x * spread;
                }
            }
        }
        const auto observedNode = observedNodeId_.load(std::memory_order_relaxed);
        if (observedNode == nodeId) {
            const auto& source = nodeOutputs[0];
            constexpr std::size_t kTracePoints = 512;
            const std::size_t srcSize = source.size();
            const std::size_t points = std::min<std::size_t>(kTracePoints, srcSize);
            auto trace = std::make_shared<std::vector<float>>();
            trace->resize(points);
            float peak = 0.0f;
            if (points > 0 && srcSize > 0) {
                for (std::size_t i = 0; i < points; ++i) {
                    const std::size_t idx = (i * srcSize) / points;
                    const float sample = source[std::min(idx, srcSize - 1)];
                    (*trace)[i] = sample;
                    peak = std::max(peak, std::abs(sample));
                }
            }
            observedNodePeak_.store(peak, std::memory_order_relaxed);
            std::atomic_store(&observedNodeTrace_, std::shared_ptr<const std::vector<float>>(trace));
        }
        return;
    } else if (procIt == processors_.end()) {
        return;
    }

    if (nodeSpec->typeName == "Mix") {
        const int availableInputs = static_cast<int>(nodeSpec->inputs.size());
        const int fallbackInputs = std::clamp(availableInputs, 2, 8);
        const int requestedInputs = std::clamp(static_cast<int>(std::lround(paramFor("inlets", static_cast<float>(fallbackInputs)))), 2, 8);
        const int activeInputs = std::clamp(requestedInputs, 2, std::max(2, availableInputs));

        for (int i = 0; i < numSamples; ++i) {
            nodeOutputs[0][static_cast<std::size_t>(i)] = 0.0f;
        }

        for (int port = 0; port < activeInputs; ++port) {
            gatherInputForPort(graph, nodeId, static_cast<neurons::engine::core::PortIndex>(port), inputA_.data(), numSamples);
            for (int i = 0; i < numSamples; ++i) {
                nodeOutputs[0][static_cast<std::size_t>(i)] += inputA_[static_cast<std::size_t>(i)];
            }
        }

        const float norm = 1.0f / static_cast<float>(activeInputs);
        for (int i = 0; i < numSamples; ++i) {
            nodeOutputs[0][static_cast<std::size_t>(i)] *= norm;
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
                nodeOutputs[0][static_cast<std::size_t>(i)] = selectB ? inputB_[static_cast<std::size_t>(i)]
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
            nodeOutputs[0][static_cast<std::size_t>(i)] = selectB ? inputB_[static_cast<std::size_t>(i)]
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
    } else if (auto* bank = dynamic_cast<neurons::engine::nodes::AllpassBankNode*>(procIt->second.get()); bank != nullptr) {
        bank->setBaseDelayMs(paramFor("delay_ms", 4.0f));
        bank->setFeedback(paramFor("feedback", 0.6f));
        bank->setSpread(paramFor("spread", 0.35f));
    } else if (auto* comb = dynamic_cast<neurons::engine::nodes::CombFilterNode*>(procIt->second.get()); comb != nullptr) {
        comb->setDelayMs(paramFor("delay_ms", 18.0f));
        comb->setFeedback(paramFor("feedback", 0.75f));
        comb->setDamping(paramFor("damping", 0.2f));
    } else if (auto* diff = dynamic_cast<neurons::engine::nodes::DiffusionBlockNode*>(procIt->second.get()); diff != nullptr) {
        diff->setSizeMs(paramFor("size_ms", 12.0f));
        diff->setFeedback(paramFor("feedback", 0.6f));
        diff->setMix(paramFor("mix", 0.5f));
    } else if (auto* tap = dynamic_cast<neurons::engine::nodes::FeedbackTapNode*>(procIt->second.get()); tap != nullptr) {
        tap->setLoopMs(paramFor("loop_ms", 250.0f));
        tap->setReinject(paramFor("reinject", 0.35f));
        tap->setFreeze(paramFor("freeze", 0.0f) >= 0.5f);
    } else if (auto* shGated = dynamic_cast<neurons::engine::nodes::SampleHoldGatedNode*>(procIt->second.get()); shGated != nullptr) {
        shGated->setThreshold(paramFor("threshold", 0.5f));
    } else if (auto* shClocked = dynamic_cast<neurons::engine::nodes::SampleHoldClockedNode*>(procIt->second.get()); shClocked != nullptr) {
        shClocked->setLowHigh(paramFor("low", 0.3f), paramFor("high", 0.7f));
    } else if (auto* shSlew = dynamic_cast<neurons::engine::nodes::SampleHoldSlewNode*>(procIt->second.get()); shSlew != nullptr) {
        shSlew->setLowHigh(paramFor("low", 0.3f), paramFor("high", 0.7f));
        shSlew->setSlewMs(paramFor("slew_ms", 8.0f));
    } else if (auto* shQuant = dynamic_cast<neurons::engine::nodes::SampleHoldQuantizedNode*>(procIt->second.get()); shQuant != nullptr) {
        shQuant->setThreshold(paramFor("threshold", 0.5f));
        shQuant->setSteps(paramFor("steps", 12.0f));
    } else if (auto* player = dynamic_cast<neurons::engine::nodes::SamplePlayerWavNode*>(procIt->second.get()); player != nullptr) {
        player->setBaseRate(paramFor("rate", 1.0f));
        player->setCvOctaves(paramFor("cv_octaves", 1.0f));
    } else if (auto* schmitt = dynamic_cast<neurons::engine::nodes::SchmittTriggerNode*>(procIt->second.get()); schmitt != nullptr) {
        schmitt->setThreshold(paramFor("threshold", 0.5f));
        schmitt->setHysteresis(paramFor("hysteresis", 0.2f));
    } else if (auto* window = dynamic_cast<neurons::engine::nodes::WindowComparatorNode*>(procIt->second.get()); window != nullptr) {
        window->setCenter(paramFor("center", 0.0f));
        window->setWidth(paramFor("width", 0.5f));
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
                            std::span<float>(nodeOutputs[0].data(), static_cast<std::size_t>(numSamples)));

    if (auto* probe = dynamic_cast<neurons::engine::nodes::ScopeProbeNode*>(procIt->second.get()); probe != nullptr) {
        latestScopeProbePeak_.store(probe->lastPeak(), std::memory_order_relaxed);
        latestScopeProbeNodeId_.store(nodeId, std::memory_order_relaxed);
        const auto& source = nodeOutputs[0];
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

    const auto observedNode = observedNodeId_.load(std::memory_order_relaxed);
    if (observedNode == nodeId) {
        const auto& source = nodeOutputs[0];
        constexpr std::size_t kTracePoints = 512;
        const std::size_t srcSize = source.size();
        const std::size_t points = std::min<std::size_t>(kTracePoints, srcSize);
        auto trace = std::make_shared<std::vector<float>>();
        trace->resize(points);
        float peak = 0.0f;
        if (points > 0 && srcSize > 0) {
            for (std::size_t i = 0; i < points; ++i) {
                const std::size_t idx = (i * srcSize) / points;
                const float sample = source[std::min(idx, srcSize - 1)];
                (*trace)[i] = sample;
                peak = std::max(peak, std::abs(sample));
            }
        }
        observedNodePeak_.store(peak, std::memory_order_relaxed);
        std::atomic_store(&observedNodeTrace_, std::shared_ptr<const std::vector<float>>(trace));
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

        const auto& nodeOutputs = srcIt->second;
        const std::size_t port = static_cast<std::size_t>(c.fromPort);
        if (port >= nodeOutputs.size()) {
            continue;
        }
        const auto& source = nodeOutputs[port];
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
