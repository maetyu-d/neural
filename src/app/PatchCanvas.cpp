#include "PatchCanvas.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_set>

namespace neurons::app {

namespace {
constexpr float kNodeWidth = 132.0f;
constexpr float kNodeHeight = 54.0f;
constexpr float kPortRadius = 5.0f;
constexpr float kNodeHeaderHeight = 20.0f;
constexpr float kPortStartY = 30.0f;
constexpr float kPortSpacingY = 11.0f;

bool isConverterFeasible(neurons::engine::core::SignalType from, neurons::engine::core::SignalType to) {
    return from != to;
}

bool isModulationInputName(const std::string& name) {
    auto lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    static constexpr const char* kAudioPathNames[] = {
        "in", "in1", "in2", "in3", "in4", "in_l", "in_r", "a", "b", "pre",
    };
    for (const auto* n : kAudioPathNames) {
        if (lower == n) {
            return false;
        }
    }

    static constexpr const char* kModTokens[] = {
        "mod", "cv", "mix", "cutoff", "time", "delay", "feedback", "damping", "spread",
        "size", "drive", "bias", "threshold", "center", "prob", "rate", "freeze", "aux",
        "side", "phase",
    };
    for (const auto* token : kModTokens) {
        if (lower.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool isModulationInputPort(const neurons::engine::core::NodeSpec* spec, neurons::engine::core::PortIndex port) {
    if (spec == nullptr) {
        return false;
    }
    const auto it = std::find_if(spec->inputs.begin(), spec->inputs.end(), [&](const auto& p) {
        return p.index == port;
    });
    if (it == spec->inputs.end()) {
        return false;
    }
    return isModulationInputName(it->name);
}

bool isBitVisualizerNode(const std::string& typeName) {
    if (typeName.rfind("Bit", 0) == 0) {
        return true;
    }
    return typeName == "ShiftLeft" || typeName == "ShiftRight" || typeName == "RotateLeft" ||
           typeName == "RotateRight" || typeName == "Popcount" || typeName == "Parity" ||
           typeName == "LeadingZeros" || typeName == "TrailingZeros" || typeName == "ByteSwap";
}
} // namespace

PatchCanvas::PatchCanvas(neurons::engine::rt::AudioEngine& engine)
    : engine_(engine) {
    setWantsKeyboardFocus(true);
    syncFromGraph();
}

void PatchCanvas::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(132, 138, 146));

    g.setColour(juce::Colour::fromRGBA(96, 104, 114, 128));
    const float worldLeft = screenToWorld({0.0f, 0.0f}).x;
    const float worldRight = screenToWorld({static_cast<float>(getWidth()), 0.0f}).x;
    const float worldTop = screenToWorld({0.0f, 0.0f}).y;
    const float worldBottom = screenToWorld({0.0f, static_cast<float>(getHeight())}).y;
    constexpr float gridStep = 40.0f;

    const float startX = std::floor(worldLeft / gridStep) * gridStep;
    const float startY = std::floor(worldTop / gridStep) * gridStep;
    for (float x = startX; x <= worldRight; x += gridStep) {
        const float sx = worldToScreen({x, 0.0f}).x;
        g.drawVerticalLine(juce::roundToInt(sx), 0.0f, static_cast<float>(getHeight()));
    }
    for (float y = startY; y <= worldBottom; y += gridStep) {
        const float sy = worldToScreen({0.0f, y}).y;
        g.drawHorizontalLine(juce::roundToInt(sy), 0.0f, static_cast<float>(getWidth()));
    }

    const auto& graph = engine_.graph();

    for (const auto& connection : graph.connections()) {
        const auto fromIt = visuals_.find(connection.fromNode);
        const auto toIt = visuals_.find(connection.toNode);
        if (fromIt == visuals_.end() || toIt == visuals_.end()) {
            continue;
        }

        const auto start = outputPortPos(fromIt->second, connection.fromPort);
        const auto end = inputPortPos(toIt->second, connection.toPort);
        const auto dx = std::max(28.0f, std::abs(end.x - start.x) * 0.4f);

        juce::Path path;
        path.startNewSubPath(start);
        path.cubicTo(start.translated(dx, 0.0f), end.translated(-dx, 0.0f), end);

        const auto* toSpec = graph.getNode(connection.toNode);
        const bool modConnection = isModulationInputPort(toSpec, connection.toPort);
        const bool selectedConnection = isConnectionSelected(connection);
        g.setColour(selectedConnection ? (modConnection ? juce::Colour::fromRGB(255, 214, 156)
                                                        : juce::Colour::fromRGB(168, 210, 248))
                                       : (modConnection ? juce::Colour::fromRGB(244, 176, 94)
                                                        : juce::Colour::fromRGB(136, 182, 226)));
        g.strokePath(path, juce::PathStrokeType(selectedConnection ? 2.2f : 1.6f));
    }

    if (dragFromPort_.has_value()) {
        const auto fromNodeIt = visuals_.find(dragFromPort_->nodeId);
        if (fromNodeIt != visuals_.end()) {
            const auto start = dragFromPort_->isInput
                                   ? inputPortPos(fromNodeIt->second, dragFromPort_->port)
                                   : outputPortPos(fromNodeIt->second, dragFromPort_->port);
            const auto end = dragPoint_;
            const auto dx = std::max(28.0f, std::abs(end.x - start.x) * 0.4f);

            juce::Path preview;
            preview.startNewSubPath(start);
            preview.cubicTo(start.translated(dx, 0.0f), end.translated(-dx, 0.0f), end);

            g.setColour(juce::Colour::fromRGBA(178, 220, 252, 210));
            g.strokePath(preview, juce::PathStrokeType(1.7f));
        }
    }

    if (boxSelectActive_) {
        const juce::Rectangle<float> r(boxSelectStart_.x,
                                       boxSelectStart_.y,
                                       boxSelectCurrent_.x - boxSelectStart_.x,
                                       boxSelectCurrent_.y - boxSelectStart_.y);
        const auto box = r.getSmallestIntegerContainer();
        g.setColour(juce::Colour::fromRGBA(82, 116, 154, 28));
        g.fillRect(box);
        g.setColour(juce::Colour::fromRGBA(82, 116, 154, 130));
        g.drawRect(box, 1);
    }

    for (const auto id : drawOrder_) {
        const auto nodeIt = visuals_.find(id);
        if (nodeIt == visuals_.end()) {
            continue;
        }
        const auto& node = nodeIt->second;
        const auto rect = nodeBoundsScreen(node);

        const bool selected = std::find(selected_.begin(), selected_.end(), id) != selected_.end();
        const bool isConverter = (node.typeName == "UnitConvert");
        g.setColour(isConverter ? (selected ? juce::Colour::fromRGB(248, 222, 118)
                                            : juce::Colour::fromRGB(238, 206, 92))
                                : (selected ? juce::Colour::fromRGB(222, 227, 234)
                                            : juce::Colour::fromRGB(238, 242, 246)));
        g.fillRoundedRectangle(rect, 8.0f);

        g.setColour(isConverter ? (selected ? juce::Colour::fromRGB(170, 126, 28)
                                            : juce::Colour::fromRGB(146, 108, 20))
                                : (selected ? juce::Colour::fromRGB(72, 98, 128)
                                            : juce::Colour::fromRGB(108, 116, 126)));
        g.drawRoundedRectangle(rect, 8.0f, 1.4f);

        g.setColour(juce::Colour::fromRGB(32, 36, 42));
        g.setFont(juce::FontOptions{11.0f});
        g.drawText(node.typeName,
                   rect.reduced(6.0f).toNearestInt(),
                   juce::Justification::centred,
                   true);

        if (isBitVisualizerNode(node.typeName)) {
            const auto word = engine_.latestBitWord(id);
            const std::uint16_t value = word.value_or(0u);
            const auto bar = rect.withTrimmedLeft(8.0f).withTrimmedRight(8.0f).removeFromBottom(9.0f);
            const float cellW = (bar.getWidth() - 15.0f) / 16.0f;
            for (int bit = 0; bit < 16; ++bit) {
                const float x = bar.getX() + static_cast<float>(bit) * (cellW + 1.0f);
                const juce::Rectangle<float> cell(x, bar.getY(), std::max(1.0f, cellW), 6.0f);
                const bool on = ((value >> (15 - bit)) & 0x1u) != 0u;
                g.setColour(on ? juce::Colour::fromRGB(242, 236, 184)
                               : juce::Colour::fromRGBA(104, 110, 118, 190));
                g.fillRoundedRectangle(cell, 1.5f);
            }
        }

        const auto* spec = graph.getNode(id);
        if (spec == nullptr) {
            continue;
        }

        for (const auto& in : spec->inputs) {
            const auto p = inputPortPos(node, in.index);
            bool hovered = hoverPort_.has_value() && hoverPort_->nodeId == id && hoverPort_->isInput &&
                           hoverPort_->port == in.index;
            const bool isMod = isModulationInputName(in.name);
            if (hovered) {
                g.setColour(hoverIsValid_ ? juce::Colour::fromRGB(88, 156, 112)
                                          : juce::Colour::fromRGB(170, 102, 102));
            } else if (isMod) {
                g.setColour(juce::Colour::fromRGB(214, 154, 84));
            } else {
                g.setColour(juce::Colour::fromRGB(98, 106, 118));
            }
            g.fillEllipse(p.x - kPortRadius, p.y - kPortRadius, kPortRadius * 2.0f, kPortRadius * 2.0f);
        }

        for (const auto& out : spec->outputs) {
            const auto p = outputPortPos(node, out.index);
            g.setColour(juce::Colour::fromRGB(84, 118, 154));
            g.fillEllipse(p.x - kPortRadius, p.y - kPortRadius, kPortRadius * 2.0f, kPortRadius * 2.0f);
        }
    }

    if (dragFromPort_.has_value() && !dragHint_.isEmpty()) {
        drawTooltip(g, dragPoint_.translated(14.0f, 12.0f), dragHint_);
    } else if (passiveHoverPort_.has_value()) {
        if (const auto text = describePort(*passiveHoverPort_); text.has_value()) {
            drawTooltip(g, passiveHoverPoint_.translated(14.0f, 12.0f), *text);
        }
    }
}

void PatchCanvas::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();
    mouseDownPos_ = event.position;
    lastPointerPos_ = event.position;
    passiveHoverPort_.reset();
    rightMousePanCandidate_ = event.mods.isRightButtonDown();
    rightMouseContextConsumed_ = false;

    if (event.mods.isMiddleButtonDown() || (event.mods.isAltDown() && event.mods.isLeftButtonDown())) {
        panning_ = true;
        panStartMouse_ = event.position;
        panStartOffset_ = panOffset_;
        return;
    }

    if (auto port = hitPort(event.position); port.has_value()) {
        dragFromPort_ = port;
        dragPoint_ = event.position;
        hoverPort_.reset();
        dragAnchor_.reset();
        selectedConnections_.clear();
        repaint();
        return;
    }

    dragAnchor_ = hitNode(event.position);

    if (!dragAnchor_.has_value()) {
        if (auto connection = hitConnection(event.position); connection.has_value()) {
            if (!event.mods.isShiftDown()) {
                selected_.clear();
                selectedConnections_.clear();
            }
            const auto it = std::find_if(selectedConnections_.begin(),
                                         selectedConnections_.end(),
                                         [&](const auto& c) {
                                             return c.fromNode == connection->fromNode &&
                                                    c.fromPort == connection->fromPort &&
                                                    c.toNode == connection->toNode &&
                                                    c.toPort == connection->toPort;
                                         });
            if (it == selectedConnections_.end()) {
                selectedConnections_.push_back(*connection);
            } else if (event.mods.isShiftDown()) {
                selectedConnections_.erase(it);
            }
            repaint();
            return;
        }
    }

    if (!dragAnchor_.has_value()) {
        if (!event.mods.isShiftDown()) {
            selected_.clear();
            selectedConnections_.clear();
        }
        boxSelectActive_ = true;
        boxSelectStart_ = event.position;
        boxSelectCurrent_ = event.position;
        boxSelectBase_ = selected_;
        repaint();
        return;
    }

    const auto id = dragAnchor_.value();
    if (!event.mods.isShiftDown()) {
        selectedConnections_.clear();
    }
    auto alreadySelected = std::find(selected_.begin(), selected_.end(), id) != selected_.end();
    if (!alreadySelected) {
        if (!event.mods.isShiftDown()) {
            selected_.clear();
        }
        selected_.push_back(id);
    } else if (event.mods.isShiftDown()) {
        selected_.erase(std::remove(selected_.begin(), selected_.end(), id), selected_.end());
        dragAnchor_.reset();
    }

    for (const auto selectedId : selected_) {
        auto it = visuals_.find(selectedId);
        if (it != visuals_.end()) {
            it->second.dragStart = it->second.position;
        }
    }

    drawOrder_.erase(std::remove(drawOrder_.begin(), drawOrder_.end(), id), drawOrder_.end());
    drawOrder_.push_back(id);

    repaint();
}

void PatchCanvas::mouseDrag(const juce::MouseEvent& event) {
    lastPointerPos_ = event.position;
    if (rightMousePanCandidate_ && !panning_) {
        const auto d = event.position - mouseDownPos_;
        if (d.getDistanceFrom({0.0f, 0.0f}) > 2.0f) {
            panning_ = true;
            panStartMouse_ = mouseDownPos_;
            panStartOffset_ = panOffset_;
            rightMouseContextConsumed_ = true;
            dragFromPort_.reset();
            hoverPort_.reset();
            dragAnchor_.reset();
            boxSelectActive_ = false;
            boxSelectBase_.clear();
        }
    }

    if (panning_) {
        panOffset_ = panStartOffset_ + (event.position - panStartMouse_);
        repaint();
        return;
    }

    if (dragFromPort_.has_value()) {
        dragPoint_ = event.position;
        hoverPort_ = hitPort(event.position);
        hoverIsValid_ = false;
        dragHint_.clear();

        if (hoverPort_.has_value() && hoverPort_->isInput == dragFromPort_->isInput) {
            dragHint_ = "Connect output to input";
            repaint();
            return;
        }

        if (hoverPort_.has_value()) {
            const PortRef from = dragFromPort_->isInput ? *hoverPort_ : *dragFromPort_;
            const PortRef to = dragFromPort_->isInput ? *dragFromPort_ : *hoverPort_;
            const auto can = engine_.graph().canConnect({from.nodeId, from.port, to.nodeId, to.port});
            if (can.ok) {
                hoverIsValid_ = true;
                dragHint_ = "Compatible connection";
            } else if (autoInsertConvertersEnabled_ && can.requiresExplicitConverter) {
                hoverIsValid_ = true;
                dragHint_ = "Will auto-insert UnitConvert";
            } else {
                hoverIsValid_ = false;
                dragHint_ = "Incompatible: " + can.error;
            }
        } else if (const auto portText = describePort(*dragFromPort_); portText.has_value()) {
            dragHint_ = *portText;
        }

        repaint();
        return;
    }

    if (boxSelectActive_) {
        boxSelectCurrent_ = event.position;

        const juce::Rectangle<float> rf(boxSelectStart_.x,
                                        boxSelectStart_.y,
                                        boxSelectCurrent_.x - boxSelectStart_.x,
                                        boxSelectCurrent_.y - boxSelectStart_.y);
        const auto box = rf.getSmallestIntegerContainer();

        selected_ = boxSelectBase_;
        for (const auto id : drawOrder_) {
            const auto nodeIt = visuals_.find(id);
            if (nodeIt == visuals_.end()) {
                continue;
            }
            const auto n = nodeBoundsScreen(nodeIt->second).toNearestInt();
            if (box.intersects(n) &&
                std::find(selected_.begin(), selected_.end(), id) == selected_.end()) {
                selected_.push_back(id);
            }
        }

        repaint();
        return;
    }

    if (!dragAnchor_.has_value()) {
        return;
    }

    const auto delta = (event.position - mouseDownPos_) / std::max(zoom_, 0.0001f);
    for (const auto selectedId : selected_) {
        auto it = visuals_.find(selectedId);
        if (it != visuals_.end()) {
            it->second.position = it->second.dragStart + delta;
        }
    }

    repaint();
}

void PatchCanvas::mouseUp(const juce::MouseEvent&) {
    if (rightMousePanCandidate_ && !rightMouseContextConsumed_ && !panning_) {
        if (auto port = hitPort(mouseDownPos_); port.has_value()) {
            showPortMenu(*port);
            rightMousePanCandidate_ = false;
            return;
        }
    }

    panning_ = false;
    rightMousePanCandidate_ = false;

    if (dragFromPort_.has_value() && hoverPort_.has_value()) {
        const PortRef from = dragFromPort_->isInput ? *hoverPort_ : *dragFromPort_;
        const PortRef to = dragFromPort_->isInput ? *dragFromPort_ : *hoverPort_;
        tryConnectPorts(from, to);
    }

    dragAnchor_.reset();
    boxSelectActive_ = false;
    boxSelectBase_.clear();
    dragFromPort_.reset();
    hoverPort_.reset();
    hoverIsValid_ = false;
    dragHint_.clear();
    repaint();
}

void PatchCanvas::mouseMove(const juce::MouseEvent& event) {
    lastPointerPos_ = event.position;
    if (dragFromPort_.has_value()) {
        return;
    }

    passiveHoverPort_ = hitPort(event.position);
    passiveHoverPoint_ = event.position;
    repaint();
}

void PatchCanvas::mouseExit(const juce::MouseEvent&) {
    passiveHoverPort_.reset();
    repaint();
}

void PatchCanvas::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (std::abs(wheel.deltaY) < 0.0001f) {
        return;
    }

    const auto cursor = event.position;
    const auto worldAtCursor = screenToWorld(cursor);
    const float factor = (wheel.deltaY > 0.0f) ? 1.1f : 0.9f;
    zoom_ = std::clamp(zoom_ * factor, 0.25f, 3.0f);
    panOffset_.x = cursor.x - worldAtCursor.x * zoom_;
    panOffset_.y = cursor.y - worldAtCursor.y * zoom_;
    repaint();
}

bool PatchCanvas::keyPressed(const juce::KeyPress& key) {
    if (key.getModifiers().isCommandDown()) {
        const auto ch = key.getTextCharacter();
        if (ch == 'a' || ch == 'A') {
            autoInsertConvertersEnabled_ = !autoInsertConvertersEnabled_;
            repaint();
            return true;
        }
        if (ch == 's' || ch == 'S') {
            selected_.clear();
            selectedConnections_.clear();
            for (const auto id : drawOrder_) {
                if (visuals_.find(id) != visuals_.end()) {
                    selected_.push_back(id);
                }
            }
            const auto& connections = engine_.graph().connections();
            selectedConnections_.assign(connections.begin(), connections.end());
            repaint();
            return true;
        }
        if (ch == 'c' || ch == 'C') {
            copySelection();
            return true;
        }
        if (ch == 'v' || ch == 'V') {
            pasteSelection();
            return true;
        }
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        deleteSelected();
        return true;
    }
    return false;
}

void PatchCanvas::addNode(const std::string& typeName) {
    std::scoped_lock lock(engine_.graphMutex());
    auto& graph = engine_.graph();
    const auto id = nextNodeId();
    auto spec = makeSpec(id, typeName);

    if (!graph.addNode(std::move(spec))) {
        return;
    }

    VisualNode node;
    node.id = id;
    node.typeName = typeName;
    node.position = screenToWorld(
        juce::Point<float>(80.0f + static_cast<float>(visuals_.size()) * 26.0f,
                           100.0f + static_cast<float>(visuals_.size()) * 22.0f));
    visuals_[id] = node;
    drawOrder_.push_back(id);

    selected_.clear();
    selectedConnections_.clear();
    selected_.push_back(id);
    pasteCount_ = 0;
    repaint();
}

void PatchCanvas::connectSelected() {
    std::scoped_lock lock(engine_.graphMutex());
    if (selected_.size() != 2) {
        return;
    }

    tryConnectPorts({selected_[0], 0, false}, {selected_[1], 0, true});
    repaint();
}

void PatchCanvas::deleteSelected() {
    std::scoped_lock lock(engine_.graphMutex());
    auto& graph = engine_.graph();
    for (const auto& c : selectedConnections_) {
        graph.removeConnection(c);
    }
    for (const auto id : selected_) {
        graph.removeNode(id);
        visuals_.erase(id);
        drawOrder_.erase(std::remove(drawOrder_.begin(), drawOrder_.end(), id), drawOrder_.end());
    }
    selected_.clear();
    selectedConnections_.clear();
    repaint();
}

void PatchCanvas::clearGraph() {
    std::scoped_lock lock(engine_.graphMutex());
    engine_.graph().clear();
    visuals_.clear();
    drawOrder_.clear();
    selected_.clear();
    selectedConnections_.clear();
    repaint();
}

void PatchCanvas::syncFromGraph() {
    std::scoped_lock lock(engine_.graphMutex());
    const auto& graph = engine_.graph();

    for (const auto& [id, spec] : graph.nodes()) {
        if (auto it = visuals_.find(id); it != visuals_.end()) {
            it->second.typeName = spec.typeName;
            continue;
        }

        VisualNode node;
        node.id = id;
        node.typeName = spec.typeName;
        node.position = screenToWorld(
            juce::Point<float>(80.0f + static_cast<float>(visuals_.size()) * 40.0f,
                               80.0f + static_cast<float>(visuals_.size()) * 24.0f));
        visuals_[id] = node;
        drawOrder_.push_back(id);
    }

    for (auto it = visuals_.begin(); it != visuals_.end();) {
        if (graph.nodes().find(it->first) == graph.nodes().end()) {
            selected_.erase(std::remove(selected_.begin(), selected_.end(), it->first), selected_.end());
            drawOrder_.erase(std::remove(drawOrder_.begin(), drawOrder_.end(), it->first), drawOrder_.end());
            it = visuals_.erase(it);
        } else {
            ++it;
        }
    }

    selectedConnections_.erase(std::remove_if(selectedConnections_.begin(),
                                              selectedConnections_.end(),
                                              [&](const auto& selectedConnection) {
                                                  return std::none_of(graph.connections().begin(),
                                                                      graph.connections().end(),
                                                                      [&](const auto& c) {
                                                                          return c.fromNode == selectedConnection.fromNode &&
                                                                                 c.fromPort == selectedConnection.fromPort &&
                                                                                 c.toNode == selectedConnection.toNode &&
                                                                                 c.toPort == selectedConnection.toPort;
                                                                      });
                                              }),
                               selectedConnections_.end());

    repaint();
}

void PatchCanvas::setAutoInsertConvertersEnabled(bool enabled) {
    autoInsertConvertersEnabled_ = enabled;
}

bool PatchCanvas::autoInsertConvertersEnabled() const {
    return autoInsertConvertersEnabled_;
}

int PatchCanvas::selectedCount() const {
    return static_cast<int>(selected_.size() + selectedConnections_.size());
}

std::optional<neurons::engine::core::NodeId> PatchCanvas::singleSelectedNodeId() const {
    if (selected_.size() != 1) {
        return std::nullopt;
    }
    return selected_.front();
}

std::optional<std::string> PatchCanvas::singleSelectedNodeType() const {
    const auto id = singleSelectedNodeId();
    if (!id.has_value()) {
        return std::nullopt;
    }
    const auto* node = engine_.graph().getNode(*id);
    if (node == nullptr) {
        return std::nullopt;
    }
    return node->typeName;
}

std::unordered_map<neurons::engine::core::NodeId, juce::Point<float>> PatchCanvas::exportVisualLayout() const {
    std::unordered_map<neurons::engine::core::NodeId, juce::Point<float>> layout;
    layout.reserve(visuals_.size());
    for (const auto& [id, visual] : visuals_) {
        layout.emplace(id, visual.position);
    }
    return layout;
}

void PatchCanvas::applyVisualLayout(const std::unordered_map<neurons::engine::core::NodeId, juce::Point<float>>& layout) {
    for (auto& [id, visual] : visuals_) {
        const auto it = layout.find(id);
        if (it != layout.end()) {
            visual.position = it->second;
        }
    }
    selected_.clear();
    selectedConnections_.clear();
    repaint();
}

juce::Rectangle<float> PatchCanvas::nodeBounds(const VisualNode& node) const {
    return {node.position.x, node.position.y, kNodeWidth, kNodeHeight};
}

juce::Rectangle<float> PatchCanvas::nodeBoundsScreen(const VisualNode& node) const {
    const auto topLeft = worldToScreen(node.position);
    return {topLeft.x, topLeft.y, kNodeWidth * zoom_, kNodeHeight * zoom_};
}

juce::Point<float> PatchCanvas::worldToScreen(juce::Point<float> point) const {
    return {point.x * zoom_ + panOffset_.x, point.y * zoom_ + panOffset_.y};
}

juce::Point<float> PatchCanvas::screenToWorld(juce::Point<float> point) const {
    const float z = std::max(zoom_, 0.0001f);
    return {(point.x - panOffset_.x) / z, (point.y - panOffset_.y) / z};
}

juce::Point<float> PatchCanvas::inputPortPos(const VisualNode& node, neurons::engine::core::PortIndex port) const {
    const auto r = nodeBounds(node);
    return worldToScreen({r.getX(), r.getY() + kPortStartY + static_cast<float>(port) * kPortSpacingY});
}

juce::Point<float> PatchCanvas::outputPortPos(const VisualNode& node, neurons::engine::core::PortIndex port) const {
    const auto r = nodeBounds(node);
    return worldToScreen({r.getRight(), r.getY() + kPortStartY + static_cast<float>(port) * kPortSpacingY});
}

neurons::engine::core::NodeSpec PatchCanvas::makeSpec(neurons::engine::core::NodeId id,
                                                       const std::string& typeName) const {
    using neurons::engine::core::PortSpec;
    using neurons::engine::core::SignalType;

    neurons::engine::core::NodeSpec spec;
    spec.id = id;
    spec.typeName = typeName;

    if (typeName == "NeuronCore") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in_a"},
            PortSpec{1, SignalType::BipolarAudio, "in_b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }

    if (typeName == "Saturator") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "side"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }

    if (typeName == "Mix") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in1"},
            PortSpec{1, SignalType::BipolarAudio, "in2"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "MatrixMixer") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in1"},
            PortSpec{1, SignalType::BipolarAudio, "in2"},
            PortSpec{2, SignalType::BipolarAudio, "in3"},
            PortSpec{3, SignalType::BipolarAudio, "in4"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out1"},
            PortSpec{1, SignalType::BipolarAudio, "out2"},
            PortSpec{2, SignalType::BipolarAudio, "out3"},
            PortSpec{3, SignalType::BipolarAudio, "out4"},
        };
        return spec;
    }
    if (typeName == "Add") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "sum"},
        };
        return spec;
    }
    if (typeName == "AnalogAnd") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "and"},
        };
        return spec;
    }
    if (typeName == "AnalogOr") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "or"},
        };
        return spec;
    }
    if (typeName == "AnalogXor") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "xor"},
        };
        return spec;
    }
    if (typeName == "AnalogNand") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "nand"},
        };
        return spec;
    }
    if (typeName == "AnalogNor") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "nor"},
        };
        return spec;
    }
    if (typeName == "Multiply") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "product"},
        };
        return spec;
    }
    if (typeName == "Divide") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "quotient"},
        };
        return spec;
    }
    if (typeName == "BitAnd" || typeName == "BitOr" || typeName == "BitXor" ||
        typeName == "BitMask" || typeName == "BitPack") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b_value_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "BitNot" || typeName == "ByteSwap" || typeName == "Popcount" ||
        typeName == "Parity" || typeName == "LeadingZeros" || typeName == "TrailingZeros") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "b_value_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "ShiftLeft" || typeName == "ShiftRight" || typeName == "RotateLeft" ||
        typeName == "RotateRight" || typeName == "BitSet" || typeName == "BitClear" ||
        typeName == "BitToggle" || typeName == "BitExtract" || typeName == "BitUnpack") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "index_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "BitCrush" || typeName == "BitQuantize") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "bits_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "BitDelayPerBit") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "delay_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "Constant") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "aux_a"},
            PortSpec{1, SignalType::BipolarAudio, "aux_b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "Compare") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "gate"},
        };
        return spec;
    }
    if (typeName == "RandomGate") {
        spec.inputs = {
            PortSpec{0, SignalType::TriggerAudio, "clock"},
            PortSpec{1, SignalType::BipolarAudio, "prob_cv"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "gate"},
        };
        return spec;
    }
    if (typeName == "Switch") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "a"},
            PortSpec{1, SignalType::BipolarAudio, "b"},
            PortSpec{2, SignalType::BipolarAudio, "select"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "SlopeDetect") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "threshold_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "slope_state"},
        };
        return spec;
    }
    if (typeName == "SchmittTrigger") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "threshold_cv"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "gate"},
        };
        return spec;
    }
    if (typeName == "AdaptiveThreshold") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "base_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "spike"},
        };
        return spec;
    }
    if (typeName == "RefractoryGate") {
        spec.inputs = {
            PortSpec{0, SignalType::TriggerAudio, "trig_in"},
            PortSpec{1, SignalType::BipolarAudio, "time_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "gate"},
        };
        return spec;
    }
    if (typeName == "SpikeGenerator") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "threshold_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "spike"},
        };
        return spec;
    }
    if (typeName == "MembraneLeakCap") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "v_mem"},
        };
        return spec;
    }
    if (typeName == "DendriteSum") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in_a"},
            PortSpec{1, SignalType::BipolarAudio, "in_b"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "sum"},
        };
        return spec;
    }
    if (typeName == "DendriteNonlinearity") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "BurstNeuron") {
        spec.inputs = {
            PortSpec{0, SignalType::TriggerAudio, "trig"},
            PortSpec{1, SignalType::BipolarAudio, "burst_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::GateAudio, "burst"},
        };
        return spec;
    }

    if (typeName == "Oscillator") {
        spec.inputs = {
            PortSpec{0, SignalType::HzAudio, "freq_hz"},
            PortSpec{1, SignalType::BipolarAudio, "phase_mod"},
            PortSpec{2, SignalType::TriggerAudio, "wave_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }

    if (typeName == "Synapse") {
        spec.inputs = {{0, SignalType::BipolarAudio, "pre"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Integrator") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "state"}};
        return spec;
    }
    if (typeName == "Leak") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "leak_mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Threshold") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "threshold_mod"}};
        spec.outputs = {{0, SignalType::GateAudio, "gate"}};
        return spec;
    }
    if (typeName == "Pulse") {
        spec.inputs = {{0, SignalType::TriggerAudio, "trig"}, {1, SignalType::BipolarAudio, "pulse_mod"}};
        spec.outputs = {{0, SignalType::GateAudio, "pulse"}};
        return spec;
    }
    if (typeName == "Gate") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::GateAudio, "gate"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Slew") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "slew_mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Waveshaper") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "shape_mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Noise") {
        spec.inputs = {{0, SignalType::BipolarAudio, "seed_mod"}, {1, SignalType::BipolarAudio, "aux"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "noise"}};
        return spec;
    }
    if (typeName == "Drift") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "drift_mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "SamplePlayerWav") {
        spec.inputs = {{0, SignalType::TriggerAudio, "trig"}, {1, SignalType::BipolarAudio, "rate_cv"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "BytebeatJs") {
        spec.inputs = {{0, SignalType::TriggerAudio, "trig"}, {1, SignalType::BipolarAudio, "rate_cv"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "FeedbackTap") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::GateAudio, "freeze_cv"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "OscillatorPhase") {
        spec.inputs = {{0, SignalType::PhaseAudio, "phase_in"}, {1, SignalType::BipolarAudio, "phase_mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "PhaseOps") {
        spec.inputs = {{0, SignalType::PhaseAudio, "phase"}, {1, SignalType::BipolarAudio, "phase_mod"}};
        spec.outputs = {{0, SignalType::PhaseAudio, "phase_out"}};
        return spec;
    }
    if (typeName == "DelayShort") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::TimeAudio, "time"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "BiquadCore") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::HzAudio, "cutoff"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "SampleHold") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::TriggerAudio, "sample"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "SampleHoldGated") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::GateAudio, "gate"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "SampleHoldClocked") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::TriggerAudio, "clock"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "SampleHoldSlew") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::TriggerAudio, "clock"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "SampleHoldQuantized") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::GateAudio, "gate"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "CrossfadeVCA") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in_a"}, {1, SignalType::BipolarAudio, "mix"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Allpass") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "AllpassBank") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "CombFilter") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "DiffusionBlock") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Invert") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "gain_mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "Counter") {
        spec.inputs = {{0, SignalType::BipolarAudio, "clock"}, {1, SignalType::BipolarAudio, "reset"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "count"}};
        return spec;
    }
    if (typeName == "Modulo") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "mod"}};
        spec.outputs = {{0, SignalType::BipolarAudio, "out"}};
        return spec;
    }
    if (typeName == "WindowComparator") {
        spec.inputs = {{0, SignalType::BipolarAudio, "in"}, {1, SignalType::BipolarAudio, "center_cv"}};
        spec.outputs = {{0, SignalType::GateAudio, "window_gate"}};
        return spec;
    }

    if (typeName == "UnitConvert") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "scale_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }

    if (typeName == "ScopeProbe") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
            PortSpec{1, SignalType::BipolarAudio, "monitor_mod"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "through"},
        };
        return spec;
    }

    if (typeName == "OutputStereo") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in_l"},
            PortSpec{1, SignalType::BipolarAudio, "in_r"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "tap"},
        };
        return spec;
    }
    if (typeName == "OSCInput") {
        spec.inputs = {};
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "out"},
        };
        return spec;
    }
    if (typeName == "OSCOutput") {
        spec.inputs = {
            PortSpec{0, SignalType::BipolarAudio, "in"},
        };
        spec.outputs = {
            PortSpec{0, SignalType::BipolarAudio, "through"},
        };
        return spec;
    }

    spec.inputs = {
        PortSpec{0, SignalType::BipolarAudio, "in"},
        PortSpec{1, SignalType::BipolarAudio, "mod"},
    };
    spec.outputs = {
        PortSpec{0, SignalType::BipolarAudio, "out"},
    };
    return spec;
}

neurons::engine::core::NodeId PatchCanvas::nextNodeId() const {
    neurons::engine::core::NodeId maxId = 0;
    for (const auto& [id, _] : engine_.graph().nodes()) {
        maxId = std::max(maxId, id);
    }
    return maxId + 1;
}

std::optional<neurons::engine::core::NodeId> PatchCanvas::hitNode(juce::Point<float> pos) const {
    for (auto it = drawOrder_.rbegin(); it != drawOrder_.rend(); ++it) {
        const auto nodeIt = visuals_.find(*it);
        if (nodeIt != visuals_.end() && nodeBoundsScreen(nodeIt->second).contains(pos)) {
            return *it;
        }
    }
    return std::nullopt;
}

std::optional<neurons::engine::core::Connection> PatchCanvas::hitConnection(juce::Point<float> pos) const {
    const auto& graph = engine_.graph();
    for (auto it = graph.connections().rbegin(); it != graph.connections().rend(); ++it) {
        const auto& connection = *it;
        const auto fromIt = visuals_.find(connection.fromNode);
        const auto toIt = visuals_.find(connection.toNode);
        if (fromIt == visuals_.end() || toIt == visuals_.end()) {
            continue;
        }

        const auto start = outputPortPos(fromIt->second, connection.fromPort);
        const auto end = inputPortPos(toIt->second, connection.toPort);
        const auto dx = std::max(28.0f, std::abs(end.x - start.x) * 0.4f);

        juce::Path curve;
        curve.startNewSubPath(start);
        curve.cubicTo(start.translated(dx, 0.0f), end.translated(-dx, 0.0f), end);

        juce::Path stroked;
        juce::PathStrokeType(8.0f).createStrokedPath(stroked, curve);
        if (stroked.contains(pos)) {
            return connection;
        }
    }
    return std::nullopt;
}

std::optional<PatchCanvas::PortRef> PatchCanvas::hitPort(juce::Point<float> pos) const {
    const auto& graph = engine_.graph();
    for (auto itOrder = drawOrder_.rbegin(); itOrder != drawOrder_.rend(); ++itOrder) {
        const auto id = *itOrder;
        const auto nodeIt = visuals_.find(id);
        if (nodeIt == visuals_.end()) {
            continue;
        }
        const auto& node = nodeIt->second;
        const auto* spec = graph.getNode(id);
        if (spec == nullptr) {
            continue;
        }

        for (const auto& in : spec->inputs) {
            if (inputPortPos(node, in.index).getDistanceFrom(pos) <= (kPortRadius + 3.0f)) {
                return PortRef{id, in.index, true};
            }
        }

        for (const auto& out : spec->outputs) {
            if (outputPortPos(node, out.index).getDistanceFrom(pos) <= (kPortRadius + 3.0f)) {
                return PortRef{id, out.index, false};
            }
        }
    }

    return std::nullopt;
}

bool PatchCanvas::tryConnectPorts(const PortRef& from, const PortRef& to) {
    std::scoped_lock lock(engine_.graphMutex());
    if (from.isInput || !to.isInput) {
        return false;
    }

    auto& graph = engine_.graph();
    const auto result = graph.addConnection({from.nodeId, from.port, to.nodeId, to.port});
    if (result.ok) {
        return true;
    }

    if (result.requiresExplicitConverter && autoInsertConvertersEnabled_) {
        return tryInsertConverter(from, to);
    }

    return false;
}

bool PatchCanvas::tryInsertConverter(const PortRef& from, const PortRef& to) {
    const auto* fromSpec = engine_.graph().getNode(from.nodeId);
    const auto* toSpec = engine_.graph().getNode(to.nodeId);
    if (fromSpec == nullptr || toSpec == nullptr) {
        return false;
    }

    auto fromPortIt = std::find_if(fromSpec->outputs.begin(), fromSpec->outputs.end(), [&](const auto& p) {
        return p.index == from.port;
    });
    auto toPortIt = std::find_if(toSpec->inputs.begin(), toSpec->inputs.end(), [&](const auto& p) {
        return p.index == to.port;
    });
    if (fromPortIt == fromSpec->outputs.end() || toPortIt == toSpec->inputs.end()) {
        return false;
    }

    if (!isConverterFeasible(fromPortIt->type, toPortIt->type)) {
        return false;
    }

    const auto converterId = nextNodeId();
    auto converter = makeSpec(converterId, "UnitConvert");
    converter.inputs[0].type = fromPortIt->type;
    converter.outputs[0].type = toPortIt->type;

    auto& graph = engine_.graph();
    if (!graph.addNode(converter)) {
        return false;
    }

    auto fromVisualIt = visuals_.find(from.nodeId);
    auto toVisualIt = visuals_.find(to.nodeId);
    if (fromVisualIt != visuals_.end() && toVisualIt != visuals_.end()) {
        VisualNode v;
        v.id = converterId;
        v.typeName = "UnitConvert";
        v.position = (fromVisualIt->second.position + toVisualIt->second.position) * 0.5f;
        visuals_[converterId] = v;
        drawOrder_.push_back(converterId);
    }

    const auto a = graph.addConnection({from.nodeId, from.port, converterId, 0});
    const auto b = graph.addConnection({converterId, 0, to.nodeId, to.port});
    return a.ok && b.ok;
}

bool PatchCanvas::insertConverterAtPort(const PortRef& portRef) {
    std::scoped_lock lock(engine_.graphMutex());
    auto type = portSignalType(portRef);
    if (!type.has_value()) {
        return false;
    }

    const auto converterId = nextNodeId();
    auto converter = makeSpec(converterId, "UnitConvert");
    converter.inputs[0].type = *type;
    converter.outputs[0].type = *type;
    converter.inputs[1].type = *type;

    auto& graph = engine_.graph();
    if (!graph.addNode(converter)) {
        return false;
    }

    auto hostIt = visuals_.find(portRef.nodeId);
    VisualNode v;
    v.id = converterId;
    v.typeName = "UnitConvert";
    if (hostIt != visuals_.end()) {
        const auto hostBounds = nodeBounds(hostIt->second);
        v.position = portRef.isInput ? hostBounds.getTopLeft().translated(-220.0f, 0.0f)
                                     : hostBounds.getTopRight().translated(60.0f, 0.0f);
    } else {
        v.position = {100.0f, 100.0f};
    }
    visuals_[converterId] = v;
    drawOrder_.push_back(converterId);

    if (portRef.isInput) {
        std::vector<neurons::engine::core::Connection> toRewire;
        for (const auto& c : graph.connections()) {
            if (c.toNode == portRef.nodeId && c.toPort == portRef.port) {
                toRewire.push_back(c);
            }
        }

        for (const auto& old : toRewire) {
            graph.removeConnection(old);
            graph.addConnection({old.fromNode, old.fromPort, converterId, 0});
        }
        graph.addConnection({converterId, 0, portRef.nodeId, portRef.port});
    } else {
        graph.addConnection({portRef.nodeId, portRef.port, converterId, 0});
    }

    return true;
}

bool PatchCanvas::disconnectAtPort(const PortRef& portRef) {
    std::scoped_lock lock(engine_.graphMutex());
    auto& graph = engine_.graph();
    std::vector<neurons::engine::core::Connection> matches;
    for (const auto& c : graph.connections()) {
        if (portRef.isInput && c.toNode == portRef.nodeId && c.toPort == portRef.port) {
            matches.push_back(c);
        }
        if (!portRef.isInput && c.fromNode == portRef.nodeId && c.fromPort == portRef.port) {
            matches.push_back(c);
        }
    }

    bool changed = false;
    for (const auto& c : matches) {
        changed = graph.removeConnection(c) || changed;
    }
    return changed;
}

bool PatchCanvas::insertProbeAtPort(const PortRef& portRef) {
    std::scoped_lock lock(engine_.graphMutex());
    auto type = portSignalType(portRef);
    if (!type.has_value()) {
        return false;
    }

    const auto probeId = nextNodeId();
    auto probe = makeSpec(probeId, "ScopeProbe");
    probe.inputs[0].type = *type;
    probe.outputs[0].type = *type;
    probe.inputs[1].type = *type;

    auto& graph = engine_.graph();
    if (!graph.addNode(probe)) {
        return false;
    }

    auto hostIt = visuals_.find(portRef.nodeId);
    VisualNode v;
    v.id = probeId;
    v.typeName = "ScopeProbe";
    if (hostIt != visuals_.end()) {
        const auto hostBounds = nodeBounds(hostIt->second);
        v.position = portRef.isInput ? hostBounds.getTopLeft().translated(-250.0f, 120.0f)
                                     : hostBounds.getTopRight().translated(80.0f, 120.0f);
    } else {
        v.position = {140.0f, 180.0f};
    }
    visuals_[probeId] = v;
    drawOrder_.push_back(probeId);

    if (portRef.isInput) {
        std::vector<neurons::engine::core::Connection> sources;
        for (const auto& c : graph.connections()) {
            if (c.toNode == portRef.nodeId && c.toPort == portRef.port) {
                sources.push_back(c);
            }
        }
        for (const auto& c : sources) {
            graph.addConnection({c.fromNode, c.fromPort, probeId, 0});
        }
    } else {
        graph.addConnection({portRef.nodeId, portRef.port, probeId, 0});
    }

    return true;
}

std::optional<neurons::engine::core::SignalType> PatchCanvas::portSignalType(const PortRef& portRef) const {
    const auto* node = engine_.graph().getNode(portRef.nodeId);
    if (node == nullptr) {
        return std::nullopt;
    }

    if (portRef.isInput) {
        const auto it = std::find_if(node->inputs.begin(), node->inputs.end(), [&](const auto& p) {
            return p.index == portRef.port;
        });
        if (it != node->inputs.end()) {
            return it->type;
        }
        return std::nullopt;
    }

    const auto it = std::find_if(node->outputs.begin(), node->outputs.end(), [&](const auto& p) {
        return p.index == portRef.port;
    });
    if (it != node->outputs.end()) {
        return it->type;
    }
    return std::nullopt;
}

std::optional<juce::String> PatchCanvas::describePort(const PortRef& portRef) const {
    const auto* node = engine_.graph().getNode(portRef.nodeId);
    if (node == nullptr) {
        return std::nullopt;
    }

    if (portRef.isInput) {
        const auto it = std::find_if(node->inputs.begin(), node->inputs.end(), [&](const auto& p) {
            return p.index == portRef.port;
        });
        if (it == node->inputs.end()) {
            return std::nullopt;
        }
        return juce::String(node->typeName) + "." + juce::String(it->name) + " : " +
               juce::String(neurons::engine::core::toString(it->type).data());
    }

    const auto it = std::find_if(node->outputs.begin(), node->outputs.end(), [&](const auto& p) {
        return p.index == portRef.port;
    });
    if (it == node->outputs.end()) {
        return std::nullopt;
    }
    return juce::String(node->typeName) + "." + juce::String(it->name) + " : " +
           juce::String(neurons::engine::core::toString(it->type).data());
}

void PatchCanvas::drawTooltip(juce::Graphics& g, juce::Point<float> point, const juce::String& text) const {
    if (text.isEmpty()) {
        return;
    }

    const auto font = juce::FontOptions{11.0f};
    g.setFont(font);
    const auto width = 280;
    const auto height = 22;
    juce::Rectangle<float> box(point.x, point.y, static_cast<float>(width), static_cast<float>(height));

    if (box.getRight() > getWidth() - 8.0f) {
        box.setX(static_cast<float>(getWidth()) - box.getWidth() - 8.0f);
    }
    if (box.getBottom() > getHeight() - 8.0f) {
        box.setY(static_cast<float>(getHeight()) - box.getHeight() - 8.0f);
    }

    g.setColour(juce::Colour::fromRGBA(170, 176, 184, 236));
    g.fillRoundedRectangle(box, 6.0f);
    g.setColour(juce::Colour::fromRGB(122, 130, 140));
    g.drawRoundedRectangle(box, 6.0f, 1.0f);
    g.setColour(juce::Colour::fromRGB(40, 44, 50));
    g.drawText(text, box.reduced(8.0f).toNearestInt(), juce::Justification::centredLeft, true);
}

void PatchCanvas::showPortMenu(const PortRef& portRef) {
    juce::PopupMenu menu;
    menu.addItem(1, "Insert Converter");
    menu.addItem(2, "Disconnect");
    menu.addItem(3, "Probe");

    juce::Component::SafePointer<PatchCanvas> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
                       [safeThis, portRef](int choice) {
                           if (safeThis == nullptr) {
                               return;
                           }

                           bool changed = false;
                           if (choice == 1) {
                               changed = safeThis->insertConverterAtPort(portRef);
                           } else if (choice == 2) {
                               changed = safeThis->disconnectAtPort(portRef);
                           } else if (choice == 3) {
                               changed = safeThis->insertProbeAtPort(portRef);
                           }

                           if (changed) {
                               safeThis->repaint();
                           }
                       });
}

bool PatchCanvas::isConnectionSelected(const neurons::engine::core::Connection& connection) const {
    return std::any_of(selectedConnections_.begin(),
                       selectedConnections_.end(),
                       [&](const auto& selectedConnection) {
                           return selectedConnection.fromNode == connection.fromNode &&
                                  selectedConnection.fromPort == connection.fromPort &&
                                  selectedConnection.toNode == connection.toNode &&
                                  selectedConnection.toPort == connection.toPort;
                       });
}

void PatchCanvas::copySelection() {
    std::scoped_lock lock(engine_.graphMutex());
    const auto& graph = engine_.graph();
    clipboard_ = {};

    if (!selected_.empty()) {
        juce::Point<float> anchor{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        for (const auto id : selected_) {
            const auto it = visuals_.find(id);
            if (it == visuals_.end()) {
                continue;
            }
            anchor.x = std::min(anchor.x, it->second.position.x);
            anchor.y = std::min(anchor.y, it->second.position.y);
        }
        if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y)) {
            anchor = {0.0f, 0.0f};
        }

        std::unordered_set<neurons::engine::core::NodeId> nodeSet(selected_.begin(), selected_.end());
        for (const auto id : selected_) {
            const auto* spec = graph.getNode(id);
            const auto vIt = visuals_.find(id);
            if (spec == nullptr || vIt == visuals_.end()) {
                continue;
            }
            clipboard_.nodes.push_back(ClipboardNode{
                *spec,
                vIt->second.position - anchor,
            });
        }

        for (const auto& c : graph.connections()) {
            if (nodeSet.contains(c.fromNode) && nodeSet.contains(c.toNode)) {
                clipboard_.connections.push_back(c);
            }
        }
    } else if (!selectedConnections_.empty()) {
        clipboard_.connections = selectedConnections_;
    }
}

void PatchCanvas::pasteSelection() {
    std::scoped_lock lock(engine_.graphMutex());
    auto& graph = engine_.graph();
    if (clipboard_.nodes.empty() && clipboard_.connections.empty()) {
        return;
    }

    ++pasteCount_;
    const auto pasteOrigin = screenToWorld(lastPointerPos_) +
                             juce::Point<float>(22.0f * static_cast<float>(pasteCount_),
                                                16.0f * static_cast<float>(pasteCount_));

    std::unordered_map<neurons::engine::core::NodeId, neurons::engine::core::NodeId> remap;
    std::vector<neurons::engine::core::NodeId> pastedNodes;
    std::vector<neurons::engine::core::Connection> pastedConnections;

    for (const auto& clipboardNode : clipboard_.nodes) {
        auto spec = clipboardNode.spec;
        const auto oldId = spec.id;
        spec.id = nextNodeId();
        remap[oldId] = spec.id;
        if (graph.addNode(spec)) {
            VisualNode node;
            node.id = spec.id;
            node.typeName = spec.typeName;
            node.position = pasteOrigin + clipboardNode.relativePosition;
            visuals_[spec.id] = node;
            drawOrder_.push_back(spec.id);
            pastedNodes.push_back(spec.id);
        }
    }

    for (const auto& c : clipboard_.connections) {
        neurons::engine::core::Connection toAdd = c;
        if (!remap.empty()) {
            const auto fromIt = remap.find(c.fromNode);
            const auto toIt = remap.find(c.toNode);
            if (fromIt == remap.end() || toIt == remap.end()) {
                continue;
            }
            toAdd.fromNode = fromIt->second;
            toAdd.toNode = toIt->second;
        } else {
            if (graph.getNode(toAdd.fromNode) == nullptr || graph.getNode(toAdd.toNode) == nullptr) {
                continue;
            }
        }
        const auto result = graph.addConnection(toAdd);
        if (result.ok) {
            pastedConnections.push_back(toAdd);
        }
    }

    selected_.clear();
    selectedConnections_.clear();
    selected_ = pastedNodes;
    selectedConnections_ = pastedConnections;
    repaint();
}

} // namespace neurons::app
