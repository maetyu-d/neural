#pragma once

#include "../core/GraphModel.h"
#include "../core/TopologicalScheduler.h"
#include "../dsp/CycleSolver.h"
#include "../nodes/NodeProcessor.h"

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <mutex>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace neurons::engine::rt {

class AudioEngine {
public:
    using NodeParamMap = std::unordered_map<neurons::engine::core::NodeId,
                                            std::unordered_map<std::string, float>>;

    AudioEngine();

    void prepareToPlay(int maxBlockSize, double sampleRate);
    void processBlock(float* left, float* right, int numSamples);
    void releaseResources();

    const neurons::engine::dsp::CycleConfig& cycleConfig() const;
    float lastCpuLoadPercent() const;

    neurons::engine::core::GraphModel& graph();
    std::mutex& graphMutex();
    void publishGraphSnapshot();
    void setNodeParam(neurons::engine::core::NodeId nodeId, const std::string& key, float value);
    std::optional<float> getNodeParam(neurons::engine::core::NodeId nodeId, const std::string& key) const;
    void setNodeScript(neurons::engine::core::NodeId nodeId, const std::string& key, const std::string& value);
    std::string getNodeScript(neurons::engine::core::NodeId nodeId, const std::string& key) const;
    bool loadWavFileForNode(neurons::engine::core::NodeId nodeId, const std::string& path);
    std::string sampleClipNameForNode(neurons::engine::core::NodeId nodeId) const;
    NodeParamMap getAllNodeParams() const;
    void replaceGraphAndParams(const neurons::engine::core::GraphModel& graph, const NodeParamMap& params);
    float latestScopeProbePeak() const;
    neurons::engine::core::NodeId latestScopeProbeNodeId() const;
    std::vector<float> latestScopeProbeTrace() const;
    void setObservedNode(std::optional<neurons::engine::core::NodeId> nodeId);
    std::optional<neurons::engine::core::NodeId> observedNodeId() const;
    std::vector<float> observedNodeTrace() const;
    float observedNodePeak() const;
    std::optional<std::uint16_t> latestBitWord(neurons::engine::core::NodeId nodeId) const;
    void setOscInputValue(neurons::engine::core::NodeId nodeId, float value);
    std::vector<std::pair<neurons::engine::core::NodeId, float>> latestOscOutputs() const;
    enum class ProcessorKind : std::uint8_t {
        Unknown = 0,
        Oscillator,
        BiquadCore,
        DelayShort,
        Saturator,
        Waveshaper,
        Allpass,
        AllpassBank,
        CombFilter,
        DiffusionBlock,
        FeedbackTap,
        SampleHoldGated,
        SampleHoldClocked,
        SampleHoldSlew,
        SampleHoldQuantized,
        SamplePlayerWav,
        SchmittTrigger,
        WindowComparator,
        Modulo,
        Counter,
        Constant,
        Compare,
        RandomGate,
        SlopeDetect,
        AdaptiveThreshold,
        RefractoryGate,
        SpikeGenerator,
        MembraneLeakCap,
        DendriteSum,
        DendriteNonlinearity,
        BurstNeuron,
        NeuronCore,
        ScopeProbe,
    };

private:
    struct RuntimeSnapshot {
        using ParamMap = NodeParamMap;

        neurons::engine::core::GraphModel graph;
        neurons::engine::core::ScheduledGraph schedule;
        ParamMap nodeParams;
    };
    struct SampleClip {
        std::vector<float> samples;
        double sourceRate{48000.0};
        std::string name;
    };
    struct BitDelayLine {
        std::vector<std::uint16_t> words;
        std::size_t write{0};
    };
    struct InputRef {
        neurons::engine::core::NodeId fromNode{0};
        neurons::engine::core::PortIndex fromPort{0};
    };

    void createDefaultGraphIfEmpty();
    void rebuildRuntimeGraph(const neurons::engine::core::GraphModel& graph, int numSamples);
    std::unique_ptr<neurons::engine::nodes::NodeProcessor> createProcessor(const std::string& typeName) const;
    void processNode(const neurons::engine::core::GraphModel& graph,
                     const RuntimeSnapshot::ParamMap& params,
                     neurons::engine::core::NodeId nodeId,
                     int numSamples);
    void gatherInputForPort(const neurons::engine::core::GraphModel& graph,
                            neurons::engine::core::NodeId nodeId,
                            neurons::engine::core::PortIndex port,
                            float* out,
                            int numSamples) const;
    bool hasIncomingConnection(neurons::engine::core::NodeId nodeId,
                               neurons::engine::core::PortIndex port) const;
    std::optional<neurons::engine::core::NodeId> chooseOutputNode(const neurons::engine::core::GraphModel& graph) const;

    neurons::engine::core::GraphModel graph_;
    neurons::engine::core::TopologicalScheduler scheduler_;
    neurons::engine::dsp::CycleSolver cycleSolver_;

    std::unordered_map<neurons::engine::core::NodeId, std::unique_ptr<neurons::engine::nodes::NodeProcessor>> processors_;
    std::unordered_map<neurons::engine::core::NodeId, std::vector<std::vector<float>>> outputs_;
    std::unordered_map<neurons::engine::core::NodeId, bool> switchSelectState_;
    std::vector<float> inputA_;
    std::vector<float> inputB_;
    std::vector<float> scratch_;
    std::vector<float> dryLeft_;
    std::vector<float> dryRight_;
    std::vector<float> fallbackLeft_;
    std::vector<float> fallbackRight_;
    std::vector<std::uint8_t> converged_;
    std::vector<std::uint8_t> iterationConverged_;
    std::shared_ptr<const RuntimeSnapshot> runtimeSnapshot_;
    RuntimeSnapshot::ParamMap nodeParams_;
    std::unordered_map<neurons::engine::core::NodeId, std::unordered_map<std::string, std::string>> nodeScripts_;
    std::unordered_map<neurons::engine::core::NodeId, SampleClip> sampleClips_;
    std::unordered_map<neurons::engine::core::NodeId, BitDelayLine> bitDelayLines_;
    std::unordered_map<neurons::engine::core::NodeId, std::uint16_t> bitWordsScratch_;
    std::unordered_map<neurons::engine::core::NodeId, ProcessorKind> processorKinds_;
    std::unordered_map<neurons::engine::core::NodeId, float> oscOutputScratch_;
    std::unordered_map<std::uint64_t, std::vector<InputRef>> incomingCache_;
    std::uint64_t incomingCacheRevision_{std::numeric_limits<std::uint64_t>::max()};

    int maxBlockSize_{};
    double sampleRate_{};
    std::atomic<float> lastCpuLoadPercent_{0.0f};
    std::atomic<float> latestScopeProbePeak_{0.0f};
    std::atomic<neurons::engine::core::NodeId> latestScopeProbeNodeId_{0};
    std::shared_ptr<const std::vector<float>> latestScopeProbeTrace_;
    std::atomic<neurons::engine::core::NodeId> observedNodeId_{0};
    std::atomic<float> observedNodePeak_{0.0f};
    std::shared_ptr<const std::vector<float>> observedNodeTrace_;
    std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, std::uint16_t>> latestBitWords_;
    std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>> latestOscOutputValues_;
    std::shared_ptr<const std::unordered_map<neurons::engine::core::NodeId, float>> latestOscInputValues_;
    mutable std::mutex graphMutex_;
};

} // namespace neurons::engine::rt
