#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "../engine/rt/AudioEngine.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace neurons::app {

class PatchCanvas final : public juce::Component {
public:
    explicit PatchCanvas(neurons::engine::rt::AudioEngine& engine);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void addNode(const std::string& typeName);
    void connectSelected();
    void deleteSelected();
    void clearGraph();
    void syncFromGraph();

    void setAutoInsertConvertersEnabled(bool enabled);
    bool autoInsertConvertersEnabled() const;

    int selectedCount() const;
    std::optional<neurons::engine::core::NodeId> singleSelectedNodeId() const;
    std::optional<std::string> singleSelectedNodeType() const;
    std::unordered_map<neurons::engine::core::NodeId, juce::Point<float>> exportVisualLayout() const;
    void applyVisualLayout(const std::unordered_map<neurons::engine::core::NodeId, juce::Point<float>>& layout);

private:
    struct VisualNode {
        neurons::engine::core::NodeId id{};
        std::string typeName;
        juce::Point<float> position;
        juce::Point<float> dragStart;
    };

    struct PortRef {
        neurons::engine::core::NodeId nodeId{};
        neurons::engine::core::PortIndex port{};
        bool isInput{false};
    };

    juce::Rectangle<float> nodeBounds(const VisualNode& node) const;
    juce::Rectangle<float> nodeBoundsScreen(const VisualNode& node) const;
    juce::Point<float> worldToScreen(juce::Point<float> point) const;
    juce::Point<float> screenToWorld(juce::Point<float> point) const;
    juce::Point<float> inputPortPos(const VisualNode& node, neurons::engine::core::PortIndex port) const;
    juce::Point<float> outputPortPos(const VisualNode& node, neurons::engine::core::PortIndex port) const;
    neurons::engine::core::NodeSpec makeSpec(neurons::engine::core::NodeId id, const std::string& typeName) const;
    neurons::engine::core::NodeId nextNodeId() const;
    std::optional<neurons::engine::core::NodeId> hitNode(juce::Point<float> pos) const;
    std::optional<neurons::engine::core::Connection> hitConnection(juce::Point<float> pos) const;
    std::optional<PortRef> hitPort(juce::Point<float> pos) const;
    bool tryConnectPorts(const PortRef& from, const PortRef& to);
    bool tryInsertConverter(const PortRef& from, const PortRef& to);
    bool insertConverterAtPort(const PortRef& portRef);
    bool disconnectAtPort(const PortRef& portRef);
    bool insertProbeAtPort(const PortRef& portRef);
    std::optional<neurons::engine::core::SignalType> portSignalType(const PortRef& portRef) const;
    std::optional<juce::String> describePort(const PortRef& portRef) const;
    void drawTooltip(juce::Graphics& g, juce::Point<float> point, const juce::String& text) const;
    void showPortMenu(const PortRef& portRef);
    bool isConnectionSelected(const neurons::engine::core::Connection& connection) const;
    void copySelection();
    void pasteSelection();

    neurons::engine::rt::AudioEngine& engine_;
    std::unordered_map<neurons::engine::core::NodeId, VisualNode> visuals_;
    std::vector<neurons::engine::core::NodeId> drawOrder_;
    std::vector<neurons::engine::core::NodeId> selected_;
    std::vector<neurons::engine::core::Connection> selectedConnections_;

    struct ClipboardNode {
        neurons::engine::core::NodeSpec spec;
        juce::Point<float> relativePosition;
    };
    struct ClipboardData {
        std::vector<ClipboardNode> nodes;
        std::vector<neurons::engine::core::Connection> connections;
    };
    ClipboardData clipboard_;
    int pasteCount_{0};

    std::optional<neurons::engine::core::NodeId> dragAnchor_;
    juce::Point<float> mouseDownPos_;
    juce::Point<float> lastPointerPos_;
    bool panning_{false};
    juce::Point<float> panStartMouse_;
    juce::Point<float> panStartOffset_;
    bool boxSelectActive_{false};
    juce::Point<float> boxSelectStart_;
    juce::Point<float> boxSelectCurrent_;
    std::vector<neurons::engine::core::NodeId> boxSelectBase_;
    bool rightMousePanCandidate_{false};
    bool rightMouseContextConsumed_{false};

    std::optional<PortRef> dragFromPort_;
    std::optional<PortRef> hoverPort_;
    juce::Point<float> dragPoint_;
    juce::String dragHint_;
    bool hoverIsValid_{false};

    std::optional<PortRef> passiveHoverPort_;
    juce::Point<float> passiveHoverPoint_;

    bool autoInsertConvertersEnabled_{true};
    float zoom_{1.0f};
    juce::Point<float> panOffset_{0.0f, 0.0f};
};

} // namespace neurons::app
