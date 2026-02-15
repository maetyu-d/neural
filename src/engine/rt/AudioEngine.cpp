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
#include "../nodes/DivideNode.h"
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
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>

namespace neurons::engine::rt {

namespace {
constexpr float kCycleConvergenceThreshold = 1.0e-4f;
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;
constexpr std::uint32_t kBitMask16 = 0xFFFFu;

std::uint16_t signalToWord(float x) {
    if (x >= -1.0f && x <= 1.0f) {
        const float n = std::clamp((x + 1.0f) * 0.5f, 0.0f, 1.0f);
        return static_cast<std::uint16_t>(std::lround(n * static_cast<float>(kBitMask16)));
    }
    const float v = std::clamp(std::round(x), 0.0f, static_cast<float>(kBitMask16));
    return static_cast<std::uint16_t>(v);
}

float wordToSignal(std::uint16_t w) {
    return (static_cast<float>(w) / static_cast<float>(kBitMask16)) * 2.0f - 1.0f;
}

int bitAmountFromSignal(float x, int maxAmount) {
    const int m = std::max(0, maxAmount);
    if (m == 0) {
        return 0;
    }
    if (x >= -1.0f && x <= 1.0f) {
        const float n = std::clamp((x + 1.0f) * 0.5f, 0.0f, 1.0f);
        return std::clamp(static_cast<int>(std::lround(n * static_cast<float>(m))), 0, m);
    }
    return std::clamp(static_cast<int>(std::lround(std::abs(x))), 0, m);
}

int bitsDepthFromSignal(float x) {
    const int amount = bitAmountFromSignal(x, 15);
    return std::clamp(amount + 1, 1, 16);
}

AudioEngine::ProcessorKind kindForTypeName(const std::string& typeName) {
    using K = AudioEngine::ProcessorKind;
    if (typeName == "Oscillator") return K::Oscillator;
    if (typeName == "BiquadCore") return K::BiquadCore;
    if (typeName == "DelayShort") return K::DelayShort;
    if (typeName == "Saturator") return K::Saturator;
    if (typeName == "Waveshaper") return K::Waveshaper;
    if (typeName == "Allpass") return K::Allpass;
    if (typeName == "AllpassBank") return K::AllpassBank;
    if (typeName == "CombFilter") return K::CombFilter;
    if (typeName == "DiffusionBlock") return K::DiffusionBlock;
    if (typeName == "FeedbackTap") return K::FeedbackTap;
    if (typeName == "SampleHoldGated") return K::SampleHoldGated;
    if (typeName == "SampleHoldClocked") return K::SampleHoldClocked;
    if (typeName == "SampleHoldSlew") return K::SampleHoldSlew;
    if (typeName == "SampleHoldQuantized") return K::SampleHoldQuantized;
    if (typeName == "SamplePlayerWav") return K::SamplePlayerWav;
    if (typeName == "SchmittTrigger") return K::SchmittTrigger;
    if (typeName == "WindowComparator") return K::WindowComparator;
    if (typeName == "Modulo") return K::Modulo;
    if (typeName == "Counter") return K::Counter;
    if (typeName == "Constant") return K::Constant;
    if (typeName == "Compare") return K::Compare;
    if (typeName == "RandomGate") return K::RandomGate;
    if (typeName == "SlopeDetect") return K::SlopeDetect;
    if (typeName == "AdaptiveThreshold") return K::AdaptiveThreshold;
    if (typeName == "RefractoryGate") return K::RefractoryGate;
    if (typeName == "SpikeGenerator") return K::SpikeGenerator;
    if (typeName == "MembraneLeakCap") return K::MembraneLeakCap;
    if (typeName == "DendriteSum") return K::DendriteSum;
    if (typeName == "DendriteNonlinearity") return K::DendriteNonlinearity;
    if (typeName == "BurstNeuron") return K::BurstNeuron;
    if (typeName == "NeuronCore") return K::NeuronCore;
    if (typeName == "ScopeProbe") return K::ScopeProbe;
    return K::Unknown;
}

std::uint64_t inputKey(neurons::engine::core::NodeId nodeId, neurons::engine::core::PortIndex port) {
    return (static_cast<std::uint64_t>(nodeId) << 32u) | static_cast<std::uint64_t>(port);
}

bool allConverged(const std::vector<std::uint8_t>& converged, int numSamples) {
    return std::all_of(converged.begin(), converged.begin() + numSamples, [](std::uint8_t v) {
        return v != 0U;
    });
}
} // namespace

AudioEngine::AudioEngine()
    : cycleSolver_(neurons::engine::dsp::CycleConfig{}) {
    std::atomic_store(&latestOscInputValues_,
                      std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>>(
                          std::make_shared<std::unordered_map<neurons::engine::core::NodeId, float>>()));
}

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
    bitWordsScratch_.clear();
    oscOutputScratch_.clear();

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
    std::atomic_store(
        &latestBitWords_,
        std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, std::uint16_t>>(
            std::make_shared<std::unordered_map<neurons::engine::core::NodeId, std::uint16_t>>(bitWordsScratch_)));
    std::atomic_store(
        &latestOscOutputValues_,
        std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>>(
            std::make_shared<std::unordered_map<neurons::engine::core::NodeId, float>>(oscOutputScratch_)));

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
    bitDelayLines_.clear();
    bitWordsScratch_.clear();
    oscOutputScratch_.clear();
    processorKinds_.clear();
    incomingCache_.clear();
    incomingCacheRevision_ = std::numeric_limits<std::uint64_t>::max();
    nodeScripts_.clear();
    sampleClips_.clear();
    std::atomic_store(&latestScopeProbeTrace_, std::shared_ptr<const std::vector<float>>{});
    std::atomic_store(&observedNodeTrace_, std::shared_ptr<const std::vector<float>>{});
    std::atomic_store(&latestBitWords_,
                      std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, std::uint16_t>>{});
    std::atomic_store(&latestOscOutputValues_,
                      std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>>{});
    std::atomic_store(&latestOscInputValues_,
                      std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>>(
                          std::make_shared<std::unordered_map<neurons::engine::core::NodeId, float>>()));
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

void AudioEngine::setOscInputValue(neurons::engine::core::NodeId nodeId, float value) {
    auto current = std::atomic_load(&latestOscInputValues_);
    auto next = std::make_shared<std::unordered_map<neurons::engine::core::NodeId, float>>();
    if (current != nullptr) {
        *next = *current;
    }
    (*next)[nodeId] = value;
    std::atomic_store(&latestOscInputValues_,
                      std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>>(next));
}

std::vector<std::pair<neurons::engine::core::NodeId, float>> AudioEngine::latestOscOutputs() const {
    std::vector<std::pair<neurons::engine::core::NodeId, float>> out;
    const auto values = std::atomic_load(&latestOscOutputValues_);
    if (values == nullptr) {
        return out;
    }
    out.reserve(values->size());
    for (const auto& [id, v] : *values) {
        out.emplace_back(id, v);
    }
    return out;
}

std::optional<std::uint16_t> AudioEngine::latestBitWord(neurons::engine::core::NodeId nodeId) const {
    const auto words = std::atomic_load(&latestBitWords_);
    if (words == nullptr) {
        return std::nullopt;
    }
    const auto it = words->find(nodeId);
    if (it == words->end()) {
        return std::nullopt;
    }
    return it->second;
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
    bitDelayLines_.clear();
    bitWordsScratch_.clear();
    oscOutputScratch_.clear();
    processorKinds_.clear();
    incomingCache_.clear();
    incomingCacheRevision_ = std::numeric_limits<std::uint64_t>::max();
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
    // Intentionally start from a blank canvas.
    (void)graph_;
}

void AudioEngine::rebuildRuntimeGraph(const neurons::engine::core::GraphModel& graph, int numSamples) {
    if (incomingCacheRevision_ != graph.revision()) {
        incomingCache_.clear();
        incomingCache_.reserve(graph.connections().size());
        for (const auto& c : graph.connections()) {
            incomingCache_[inputKey(c.toNode, c.toPort)].push_back(InputRef{c.fromNode, c.fromPort});
        }
        incomingCacheRevision_ = graph.revision();
    }

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
                processorKinds_[nodeId] = kindForTypeName(spec.typeName);
            }
        }
        if (processorKinds_.find(nodeId) == processorKinds_.end()) {
            processorKinds_[nodeId] = kindForTypeName(spec.typeName);
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
            bitDelayLines_.erase(it->first);
            bitWordsScratch_.erase(it->first);
            oscOutputScratch_.erase(it->first);
            processorKinds_.erase(it->first);
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
    if (typeName == "Divide") {
        return std::make_unique<DivideNode>();
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
    const auto hasInputConnection = [&](neurons::engine::core::PortIndex port) {
        return hasIncomingConnection(nodeId, port);
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
    }

    const auto& type = nodeSpec->typeName;
    const bool isBitOpType =
        type == "BitAnd" || type == "BitOr" || type == "BitXor" || type == "BitNot" ||
        type == "ShiftLeft" || type == "ShiftRight" || type == "RotateLeft" || type == "RotateRight" ||
        type == "BitMask" || type == "BitSet" || type == "BitClear" || type == "BitToggle" ||
        type == "BitExtract" || type == "BitPack" || type == "BitUnpack" || type == "Popcount" ||
        type == "Parity" || type == "LeadingZeros" || type == "TrailingZeros" || type == "ByteSwap" ||
        type == "BitCrush" || type == "BitQuantize" || type == "BitDelayPerBit";

    if (isBitOpType) {
        gatherInputForPort(graph, nodeId, 0, inputA_.data(), numSamples);
        gatherInputForPort(graph, nodeId, 1, inputB_.data(), numSamples);
        const bool hasBConnection = hasInputConnection(1);
        if (!hasBConnection) {
            const float fallback = paramFor("b_value", 0.0f);
            std::fill_n(inputB_.begin(), numSamples, fallback);
        }

        auto* out = nodeOutputs[0].data();
        BitDelayLine* delayLine = nullptr;
        if (type == "BitDelayPerBit") {
            auto& line = bitDelayLines_[nodeId];
            if (line.words.empty()) {
                line.words.assign(8192, 0u);
                line.write = 0;
            }
            delayLine = &line;
        }

        std::uint16_t latestWord = 0u;
        for (int i = 0; i < numSamples; ++i) {
            const float a = inputA_[static_cast<std::size_t>(i)];
            const float b = inputB_[static_cast<std::size_t>(i)];
            const std::uint16_t aWord = signalToWord(a);
            const std::uint16_t bWord = signalToWord(b);
            std::uint16_t outWord = 0u;

            if (type == "BitAnd") outWord = static_cast<std::uint16_t>(aWord & bWord);
            else if (type == "BitOr") outWord = static_cast<std::uint16_t>(aWord | bWord);
            else if (type == "BitXor") outWord = static_cast<std::uint16_t>(aWord ^ bWord);
            else if (type == "BitNot") outWord = static_cast<std::uint16_t>(~aWord);
            else if (type == "ShiftLeft") {
                const int s = bitAmountFromSignal(b, 15);
                outWord = static_cast<std::uint16_t>((static_cast<std::uint32_t>(aWord) << s) & kBitMask16);
            } else if (type == "ShiftRight") {
                const int s = bitAmountFromSignal(b, 15);
                outWord = static_cast<std::uint16_t>(aWord >> s);
            } else if (type == "RotateLeft") {
                const int s = bitAmountFromSignal(b, 15);
                outWord = std::rotl(aWord, s);
            } else if (type == "RotateRight") {
                const int s = bitAmountFromSignal(b, 15);
                outWord = std::rotr(aWord, s);
            } else if (type == "BitMask") outWord = static_cast<std::uint16_t>(aWord & bWord);
            else if (type == "BitSet") {
                const int idx = bitAmountFromSignal(b, 15);
                outWord = static_cast<std::uint16_t>(aWord | (1u << idx));
            } else if (type == "BitClear") {
                const int idx = bitAmountFromSignal(b, 15);
                outWord = static_cast<std::uint16_t>(aWord & ~(1u << idx));
            } else if (type == "BitToggle") {
                const int idx = bitAmountFromSignal(b, 15);
                outWord = static_cast<std::uint16_t>(aWord ^ (1u << idx));
            } else if (type == "BitExtract") {
                const int idx = bitAmountFromSignal(b, 15);
                outWord = ((aWord >> idx) & 0x1u) != 0u ? 0xFFFFu : 0u;
            } else if (type == "BitPack") {
                outWord = static_cast<std::uint16_t>(((aWord & 0x00FFu) << 8) | (bWord & 0x00FFu));
            } else if (type == "BitUnpack") {
                const int which = bitAmountFromSignal(b, 1);
                const std::uint16_t byte = static_cast<std::uint16_t>(which == 0 ? (aWord & 0x00FFu)
                                                                                  : ((aWord >> 8) & 0x00FFu));
                outWord = static_cast<std::uint16_t>((byte << 8) | byte);
            } else if (type == "Popcount") {
                const int c = std::popcount(aWord);
                outWord = static_cast<std::uint16_t>(std::lround((static_cast<float>(c) / 16.0f) * static_cast<float>(kBitMask16)));
            } else if (type == "Parity") {
                outWord = (std::popcount(aWord) & 1) != 0 ? 0xFFFFu : 0u;
            } else if (type == "LeadingZeros") {
                const int c = std::countl_zero(aWord);
                outWord = static_cast<std::uint16_t>(std::lround((static_cast<float>(c) / 16.0f) * static_cast<float>(kBitMask16)));
            } else if (type == "TrailingZeros") {
                const int c = std::countr_zero(aWord);
                outWord = static_cast<std::uint16_t>(std::lround((static_cast<float>(c) / 16.0f) * static_cast<float>(kBitMask16)));
            } else if (type == "ByteSwap") {
                outWord = static_cast<std::uint16_t>(((aWord & 0x00FFu) << 8) | ((aWord & 0xFF00u) >> 8));
            } else if (type == "BitCrush") {
                const int bits = bitsDepthFromSignal(b);
                if (bits >= 16) {
                    outWord = aWord;
                } else {
                    const std::uint16_t keepMask = static_cast<std::uint16_t>(~((1u << (16 - bits)) - 1u) & kBitMask16);
                    outWord = static_cast<std::uint16_t>(aWord & keepMask);
                }
            } else if (type == "BitQuantize") {
                const int bits = bitsDepthFromSignal(b);
                const int levels = (1 << bits) - 1;
                const float n = std::clamp((a + 1.0f) * 0.5f, 0.0f, 1.0f);
                const float q = std::round(n * static_cast<float>(levels)) / static_cast<float>(levels);
                const float y = std::clamp((q * 2.0f) - 1.0f, -1.0f, 1.0f);
                out[static_cast<std::size_t>(i)] = y;
                latestWord = signalToWord(y);
                continue;
            } else if (type == "BitDelayPerBit") {
                auto& line = *delayLine;
                line.words[line.write] = aWord;
                const int base = bitAmountFromSignal(b, 64);
                std::uint16_t delayedWord = 0u;
                const std::size_t size = line.words.size();
                for (int bit = 0; bit < 16; ++bit) {
                    const std::size_t d = static_cast<std::size_t>(base * (bit + 1));
                    const std::size_t read = (line.write + size - (d % size)) % size;
                    const std::uint16_t srcWord = line.words[read];
                    if (((srcWord >> bit) & 0x1u) != 0u) {
                        delayedWord = static_cast<std::uint16_t>(delayedWord | (1u << bit));
                    }
                }
                outWord = delayedWord;
                line.write = (line.write + 1) % line.words.size();
            }

            out[static_cast<std::size_t>(i)] = wordToSignal(outWord);
            latestWord = outWord;
        }
        bitWordsScratch_[nodeId] = latestWord;
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

        const bool hasSelectConnection = hasInputConnection(2);

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

    if (nodeSpec->typeName == "OSCInput") {
        float v = 0.0f;
        if (const auto values = std::atomic_load(&latestOscInputValues_); values != nullptr) {
            if (const auto it = values->find(nodeId); it != values->end()) {
                v = it->second;
            }
        }
        std::fill_n(nodeOutputs[0].begin(), numSamples, v);
        return;
    }

    if (nodeSpec->typeName == "OSCOutput") {
        gatherInputForPort(graph, nodeId, 0, inputA_.data(), numSamples);
        std::copy_n(inputA_.begin(), numSamples, nodeOutputs[0].begin());
        if (numSamples > 0) {
            oscOutputScratch_[nodeId] = inputA_[static_cast<std::size_t>(numSamples - 1)];
        }
        return;
    }

    if (nodeSpec->typeName == "Multiply") {
        gatherInputForPort(graph, nodeId, 0, inputA_.data(), numSamples);
        gatherInputForPort(graph, nodeId, 1, inputB_.data(), numSamples);
        const bool hasBConnection = hasInputConnection(1);
        if (!hasBConnection) {
            const float multiplicand = paramFor("multiplicand", 1.0f);
            std::fill_n(inputB_.begin(), numSamples, multiplicand);
        }
        procIt->second->process(std::span<const float>(inputA_.data(), static_cast<std::size_t>(numSamples)),
                                std::span<const float>(inputB_.data(), static_cast<std::size_t>(numSamples)),
                                std::span<float>(nodeOutputs[0].data(), static_cast<std::size_t>(numSamples)));
        return;
    }

    gatherInputForPort(graph, nodeId, 0, inputA_.data(), numSamples);
    gatherInputForPort(graph, nodeId, 1, inputB_.data(), numSamples);
    if (!hasInputConnection(1)) {
        if (nodeSpec->typeName == "Add" || nodeSpec->typeName == "Compare" || nodeSpec->typeName == "Divide") {
            const float bValue = paramFor("b_value", 0.0f);
            std::fill_n(inputB_.begin(), numSamples, bValue);
        }
    }

    const auto kindIt = processorKinds_.find(nodeId);
    const auto kind = kindIt != processorKinds_.end() ? kindIt->second : ProcessorKind::Unknown;
    switch (kind) {
    case ProcessorKind::Oscillator: {
        auto* n = static_cast<neurons::engine::nodes::OscillatorNode*>(procIt->second.get());
        n->setFrequencyHz(paramFor("freq_hz", 220.0f));
        n->setWaveform(paramFor("waveform", 0.0f));
        break;
    }
    case ProcessorKind::BiquadCore:
        static_cast<neurons::engine::nodes::BiquadCoreNode*>(procIt->second.get())->setCutoffHz(paramFor("cutoff_hz", 1200.0f));
        break;
    case ProcessorKind::DelayShort:
        static_cast<neurons::engine::nodes::DelayShortNode*>(procIt->second.get())->setDelayMs(paramFor("delay_ms", 1.33f));
        break;
    case ProcessorKind::Saturator:
        static_cast<neurons::engine::nodes::SaturatorNode*>(procIt->second.get())->setDrive(paramFor("drive", 1.0f));
        break;
    case ProcessorKind::Waveshaper: {
        auto* n = static_cast<neurons::engine::nodes::WaveshaperNode*>(procIt->second.get());
        n->setDrive(paramFor("drive", 1.0f));
        n->setCurve(paramFor("curve", 0.5f));
        break;
    }
    case ProcessorKind::Allpass: {
        auto* n = static_cast<neurons::engine::nodes::AllpassNode*>(procIt->second.get());
        n->setDelayMs(paramFor("delay_ms", 6.0f));
        n->setFeedback(paramFor("feedback", 0.6f));
        break;
    }
    case ProcessorKind::AllpassBank: {
        auto* n = static_cast<neurons::engine::nodes::AllpassBankNode*>(procIt->second.get());
        n->setBaseDelayMs(paramFor("delay_ms", 4.0f));
        n->setFeedback(paramFor("feedback", 0.6f));
        n->setSpread(paramFor("spread", 0.35f));
        break;
    }
    case ProcessorKind::CombFilter: {
        auto* n = static_cast<neurons::engine::nodes::CombFilterNode*>(procIt->second.get());
        n->setDelayMs(paramFor("delay_ms", 18.0f));
        n->setFeedback(paramFor("feedback", 0.75f));
        n->setDamping(paramFor("damping", 0.2f));
        break;
    }
    case ProcessorKind::DiffusionBlock: {
        auto* n = static_cast<neurons::engine::nodes::DiffusionBlockNode*>(procIt->second.get());
        n->setSizeMs(paramFor("size_ms", 12.0f));
        n->setFeedback(paramFor("feedback", 0.6f));
        n->setMix(paramFor("mix", 0.5f));
        break;
    }
    case ProcessorKind::FeedbackTap: {
        auto* n = static_cast<neurons::engine::nodes::FeedbackTapNode*>(procIt->second.get());
        n->setLoopMs(paramFor("loop_ms", 250.0f));
        n->setReinject(paramFor("reinject", 0.35f));
        n->setFreeze(paramFor("freeze", 0.0f) >= 0.5f);
        break;
    }
    case ProcessorKind::SampleHoldGated:
        static_cast<neurons::engine::nodes::SampleHoldGatedNode*>(procIt->second.get())->setThreshold(paramFor("threshold", 0.5f));
        break;
    case ProcessorKind::SampleHoldClocked:
        static_cast<neurons::engine::nodes::SampleHoldClockedNode*>(procIt->second.get())
            ->setLowHigh(paramFor("low", 0.3f), paramFor("high", 0.7f));
        break;
    case ProcessorKind::SampleHoldSlew: {
        auto* n = static_cast<neurons::engine::nodes::SampleHoldSlewNode*>(procIt->second.get());
        n->setLowHigh(paramFor("low", 0.3f), paramFor("high", 0.7f));
        n->setSlewMs(paramFor("slew_ms", 8.0f));
        break;
    }
    case ProcessorKind::SampleHoldQuantized: {
        auto* n = static_cast<neurons::engine::nodes::SampleHoldQuantizedNode*>(procIt->second.get());
        n->setThreshold(paramFor("threshold", 0.5f));
        n->setSteps(paramFor("steps", 12.0f));
        break;
    }
    case ProcessorKind::SamplePlayerWav: {
        auto* n = static_cast<neurons::engine::nodes::SamplePlayerWavNode*>(procIt->second.get());
        n->setBaseRate(paramFor("rate", 1.0f));
        n->setCvOctaves(paramFor("cv_octaves", 1.0f));
        break;
    }
    case ProcessorKind::SchmittTrigger: {
        auto* n = static_cast<neurons::engine::nodes::SchmittTriggerNode*>(procIt->second.get());
        n->setThreshold(paramFor("threshold", 0.5f));
        n->setHysteresis(paramFor("hysteresis", 0.2f));
        break;
    }
    case ProcessorKind::WindowComparator: {
        auto* n = static_cast<neurons::engine::nodes::WindowComparatorNode*>(procIt->second.get());
        n->setCenter(paramFor("center", 0.0f));
        n->setWidth(paramFor("width", 0.5f));
        break;
    }
    case ProcessorKind::Modulo:
        static_cast<neurons::engine::nodes::ModuloNode*>(procIt->second.get())->setModulus(paramFor("modulus", 1.0f));
        break;
    case ProcessorKind::Counter: {
        auto* n = static_cast<neurons::engine::nodes::CounterNode*>(procIt->second.get());
        n->setRange(paramFor("min", 0.0f), paramFor("max", 15.0f));
        n->setWrapMode(paramFor("wrap", 1.0f) >= 0.5f);
        break;
    }
    case ProcessorKind::Constant:
        static_cast<neurons::engine::nodes::ConstantNode*>(procIt->second.get())->setValue(paramFor("value", 0.0f));
        break;
    case ProcessorKind::Compare:
        static_cast<neurons::engine::nodes::CompareNode*>(procIt->second.get())->setGreaterMode(paramFor("greater", 1.0f) >= 0.5f);
        break;
    case ProcessorKind::RandomGate: {
        auto* n = static_cast<neurons::engine::nodes::RandomGateNode*>(procIt->second.get());
        n->setProbability(paramFor("prob", 0.5f));
        n->setPulseMs(paramFor("pulse_ms", 2.0f));
        break;
    }
    case ProcessorKind::SlopeDetect:
        static_cast<neurons::engine::nodes::SlopeDetectNode*>(procIt->second.get())->setThreshold(paramFor("threshold", 1.0e-4f));
        break;
    case ProcessorKind::AdaptiveThreshold: {
        auto* n = static_cast<neurons::engine::nodes::AdaptiveThresholdNode*>(procIt->second.get());
        n->setBaseThreshold(paramFor("base_threshold", 0.5f));
        n->setAdaptAmount(paramFor("adapt", 0.25f));
        break;
    }
    case ProcessorKind::RefractoryGate: {
        auto* n = static_cast<neurons::engine::nodes::RefractoryGateNode*>(procIt->second.get());
        n->setRefractoryMs(paramFor("refractory_ms", 30.0f));
        n->setPulseMs(paramFor("pulse_ms", 1.0f));
        break;
    }
    case ProcessorKind::SpikeGenerator: {
        auto* n = static_cast<neurons::engine::nodes::SpikeGeneratorNode*>(procIt->second.get());
        n->setThreshold(paramFor("threshold", 0.5f));
        n->setPulseMs(paramFor("pulse_ms", 1.0f));
        break;
    }
    case ProcessorKind::MembraneLeakCap: {
        auto* n = static_cast<neurons::engine::nodes::MembraneLeakCapNode*>(procIt->second.get());
        n->setTauMs(paramFor("tau_ms", 20.0f));
        n->setLeak(paramFor("leak", 0.01f));
        break;
    }
    case ProcessorKind::DendriteSum:
        static_cast<neurons::engine::nodes::DendriteSumNode*>(procIt->second.get())
            ->setGains(paramFor("gain_a", 1.0f), paramFor("gain_b", 1.0f));
        break;
    case ProcessorKind::DendriteNonlinearity: {
        auto* n = static_cast<neurons::engine::nodes::DendriteNonlinearityNode*>(procIt->second.get());
        n->setDrive(paramFor("drive", 1.0f));
        n->setBias(paramFor("bias", 0.0f));
        break;
    }
    case ProcessorKind::BurstNeuron: {
        auto* n = static_cast<neurons::engine::nodes::BurstNeuronNode*>(procIt->second.get());
        n->setCount(paramFor("count", 3.0f));
        n->setIntervalMs(paramFor("interval_ms", 8.0f));
        break;
    }
    case ProcessorKind::NeuronCore: {
        auto* n = static_cast<neurons::engine::nodes::NeuronCoreNode*>(procIt->second.get());
        neurons::engine::nodes::NeuronCoreNode::Params p;
        p.gain = paramFor("gain", 1.0f);
        p.tauMs = paramFor("tau_ms", 20.0f);
        n->setParams(p);
        break;
    }
    default:
        break;
    }

    if (kind == ProcessorKind::Oscillator) {
        gatherInputForPort(graph, nodeId, 2, scratch_.data(), numSamples);
        auto* n = static_cast<neurons::engine::nodes::OscillatorNode*>(procIt->second.get());
        n->processWithWaveSelect(std::span<const float>(inputA_.data(), static_cast<std::size_t>(numSamples)),
                                 std::span<const float>(inputB_.data(), static_cast<std::size_t>(numSamples)),
                                 std::span<const float>(scratch_.data(), static_cast<std::size_t>(numSamples)),
                                 std::span<float>(nodeOutputs[0].data(), static_cast<std::size_t>(numSamples)));
        return;
    }

    procIt->second->process(std::span<const float>(inputA_.data(), static_cast<std::size_t>(numSamples)),
                            std::span<const float>(inputB_.data(), static_cast<std::size_t>(numSamples)),
                            std::span<float>(nodeOutputs[0].data(), static_cast<std::size_t>(numSamples)));

    if (kind == ProcessorKind::ScopeProbe) {
        auto* probe = static_cast<neurons::engine::nodes::ScopeProbeNode*>(procIt->second.get());
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
    (void)graph;
    if (out == nullptr || numSamples <= 0) {
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        out[i] = 0.0f;
    }

    const auto cacheIt = incomingCache_.find(inputKey(nodeId, port));
    if (cacheIt == incomingCache_.end()) {
        return;
    }

    for (const auto& ref : cacheIt->second) {
        const auto srcIt = outputs_.find(ref.fromNode);
        if (srcIt == outputs_.end()) {
            continue;
        }

        const auto& nodeOutputs = srcIt->second;
        const std::size_t fromPort = static_cast<std::size_t>(ref.fromPort);
        if (fromPort >= nodeOutputs.size()) {
            continue;
        }
        const auto& source = nodeOutputs[fromPort];
        for (int i = 0; i < numSamples; ++i) {
            out[i] += source[static_cast<std::size_t>(i)];
        }
    }
}

bool AudioEngine::hasIncomingConnection(neurons::engine::core::NodeId nodeId,
                                        neurons::engine::core::PortIndex port) const {
    return incomingCache_.find(inputKey(nodeId, port)) != incomingCache_.end();
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
