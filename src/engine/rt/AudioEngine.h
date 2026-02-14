#pragma once

#include "../core/GraphModel.h"
#include "../core/TopologicalScheduler.h"
#include "../dsp/CycleSolver.h"
#include "../nodes/NodeProcessor.h"

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
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

    int maxBlockSize_{};
    double sampleRate_{};
    std::atomic<float> lastCpuLoadPercent_{0.0f};
    std::atomic<float> latestScopeProbePeak_{0.0f};
    std::atomic<neurons::engine::core::NodeId> latestScopeProbeNodeId_{0};
    std::shared_ptr<const std::vector<float>> latestScopeProbeTrace_;
    std::atomic<neurons::engine::core::NodeId> observedNodeId_{0};
    std::atomic<float> observedNodePeak_{0.0f};
    std::shared_ptr<const std::vector<float>> observedNodeTrace_;
    mutable std::mutex graphMutex_;
};

} // namespace neurons::engine::rt
