#include "MainComponent.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace neurons::app {

namespace {
constexpr int kTopBarHeight = 46;
constexpr int kInspectorWidth = 252;

std::optional<neurons::engine::core::SignalType> parseSignalType(const juce::String& text) {
    using neurons::engine::core::SignalType;
    if (text == "BipolarAudio") return SignalType::BipolarAudio;
    if (text == "UnipolarAudio") return SignalType::UnipolarAudio;
    if (text == "GateAudio") return SignalType::GateAudio;
    if (text == "TriggerAudio") return SignalType::TriggerAudio;
    if (text == "PhaseAudio") return SignalType::PhaseAudio;
    if (text == "HzAudio") return SignalType::HzAudio;
    if (text == "TimeAudio") return SignalType::TimeAudio;
    return std::nullopt;
}
}

void ScopeStrip::setTrace(std::vector<float> trace, bool active) {
    trace_ = std::move(trace);
    active_ = active;
    repaint();
}

void ScopeStrip::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(86, 92, 100));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour::fromRGB(154, 162, 172));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    const float midY = bounds.getCentreY();
    g.setColour(juce::Colour::fromRGBA(212, 220, 232, 120));
    g.drawLine(bounds.getX() + 6.0f, midY, bounds.getRight() - 6.0f, midY, 1.0f);

    if (!active_ || trace_.size() < 2) {
        g.setColour(juce::Colour::fromRGB(230, 236, 244));
        g.setFont(juce::FontOptions{10.0f});
        g.drawText("No live probe signal", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    juce::Path waveform;
    const float left = bounds.getX() + 6.0f;
    const float right = bounds.getRight() - 6.0f;
    const float top = bounds.getY() + 6.0f;
    const float bottom = bounds.getBottom() - 6.0f;
    const float amp = (bottom - top) * 0.5f;
    const std::size_t n = trace_.size();

    for (std::size_t i = 0; i < n; ++i) {
        const float x = left + (right - left) * (static_cast<float>(i) / static_cast<float>(n - 1));
        const float y = midY - juce::jlimit(-1.0f, 1.0f, trace_[i]) * amp;
        if (i == 0) {
            waveform.startNewSubPath(x, y);
        } else {
            waveform.lineTo(x, y);
        }
    }

    g.setColour(juce::Colour::fromRGB(220, 236, 252));
    g.strokePath(waveform, juce::PathStrokeType(1.5f));
}

MainComponent::MainComponent()
    : canvas_(audioEngine_) {
    addAndMakeVisible(canvas_);

    for (auto* button : {&nodePalette_, &load_, &save_, &connect_, &remove_, &clear_, &autoConvert_}) {
        addAndMakeVisible(*button);
        button->addListener(this);
        button->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(134, 139, 147));
        button->setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(148, 154, 162));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(34, 38, 44));
        button->setColour(juce::TextButton::textColourOnId, juce::Colour::fromRGB(26, 30, 36));
    }
    nodePalette_.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(118, 135, 154));
    nodePalette_.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(128, 146, 166));
    clear_.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(152, 86, 86));
    clear_.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(168, 98, 98));
    clear_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    clear_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    addAndMakeVisible(status_);
    status_.setJustificationType(juce::Justification::centredRight);
    status_.setFont(juce::FontOptions{10.0f});
    status_.setColour(juce::Label::textColourId, juce::Colour::fromRGB(54, 58, 66));

    addAndMakeVisible(inspectorTitle_);
    addAndMakeVisible(paramALabel_);
    addAndMakeVisible(paramBLabel_);
    addAndMakeVisible(modeLabel_);
    addAndMakeVisible(paramA_);
    addAndMakeVisible(paramB_);
    addAndMakeVisible(modeToggle_);
    addAndMakeVisible(scopeLabel_);
    addAndMakeVisible(scopeStrip_);

    inspectorTitle_.setJustificationType(juce::Justification::centredLeft);
    inspectorTitle_.setFont(juce::FontOptions{12.0f});
    inspectorTitle_.setColour(juce::Label::textColourId, juce::Colour::fromRGB(40, 44, 50));
    paramALabel_.setJustificationType(juce::Justification::centredLeft);
    paramBLabel_.setJustificationType(juce::Justification::centredLeft);
    modeLabel_.setJustificationType(juce::Justification::centredLeft);
    scopeLabel_.setJustificationType(juce::Justification::centredLeft);
    paramALabel_.setColour(juce::Label::textColourId, juce::Colour::fromRGB(64, 69, 78));
    paramBLabel_.setColour(juce::Label::textColourId, juce::Colour::fromRGB(64, 69, 78));
    modeLabel_.setColour(juce::Label::textColourId, juce::Colour::fromRGB(64, 69, 78));
    scopeLabel_.setColour(juce::Label::textColourId, juce::Colour::fromRGB(64, 69, 78));

    for (auto* s : {&paramA_, &paramB_}) {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
        s->setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(102, 113, 130));
        s->setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(128, 136, 146));
        s->setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(153, 159, 168));
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(195, 200, 207));
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(138, 145, 154));
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(36, 40, 46));
        s->addListener(this);
    }
    modeToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(50, 56, 64));
    modeToggle_.addListener(this);
    scopeLabel_.setVisible(false);
    scopeStrip_.setVisible(false);

    canvas_.syncFromGraph();
    setAudioChannels(2, 2);
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        setup.bufferSize = std::max(setup.bufferSize, 2048);
        if (setup.sampleRate <= 0.0) {
            setup.sampleRate = 48000.0;
        }
        deviceManager.setAudioDeviceSetup(setup, true);
    }

    refreshInspector();
    startTimerHz(5);
}

MainComponent::~MainComponent() {
    shutdownAudio();

    for (auto* button : {&nodePalette_, &load_, &save_, &connect_, &remove_, &clear_, &autoConvert_}) {
        button->removeListener(this);
    }
    modeToggle_.removeListener(this);
    for (auto* s : {&paramA_, &paramB_}) {
        s->removeListener(this);
    }
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(156, 161, 168));

    const auto topBar = juce::Rectangle<int>(0, 0, getWidth(), kTopBarHeight);
    g.setColour(juce::Colour::fromRGB(168, 173, 180));
    g.fillRect(topBar);
    g.setColour(juce::Colour::fromRGB(132, 138, 146));
    g.drawLine(0.0f, static_cast<float>(kTopBarHeight), static_cast<float>(getWidth()), static_cast<float>(kTopBarHeight), 1.0f);

    const auto inspectorX = getWidth() - kInspectorWidth;
    g.setColour(juce::Colour::fromRGB(176, 181, 188));
    g.fillRect(inspectorX, kTopBarHeight, kInspectorWidth, getHeight() - kTopBarHeight);
    g.setColour(juce::Colour::fromRGB(132, 138, 146));
    g.drawLine(static_cast<float>(inspectorX), static_cast<float>(kTopBarHeight), static_cast<float>(inspectorX), static_cast<float>(getHeight()), 1.0f);
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    auto top = area.removeFromTop(kTopBarHeight).reduced(8, 7);

    const int buttonW = 110;
    const int buttonGap = 6;
    for (auto* button : {&nodePalette_, &load_, &save_, &connect_, &remove_, &clear_, &autoConvert_}) {
        button->setBounds(top.removeFromLeft(buttonW));
        top.removeFromLeft(buttonGap);
    }

    status_.setBounds(top);

    auto inspector = area.removeFromRight(kInspectorWidth).reduced(10);
    inspectorTitle_.setBounds(inspector.removeFromTop(24));
    inspector.removeFromTop(6);

    paramALabel_.setBounds(inspector.removeFromTop(18));
    paramA_.setBounds(inspector.removeFromTop(26));
    inspector.removeFromTop(10);

    paramBLabel_.setBounds(inspector.removeFromTop(18));
    paramB_.setBounds(inspector.removeFromTop(26));
    inspector.removeFromTop(10);

    modeLabel_.setBounds(inspector.removeFromTop(18));
    modeToggle_.setBounds(inspector.removeFromTop(24));
    inspector.removeFromTop(10);
    scopeLabel_.setBounds(inspector.removeFromTop(18));
    scopeStrip_.setBounds(inspector.removeFromTop(88));

    canvas_.setBounds(area);
}

void MainComponent::buttonClicked(juce::Button* button) {
    if (button == &nodePalette_) {
        showNodePaletteMenu();
    } else if (button == &load_) {
        loadProject();
    } else if (button == &save_) {
        saveProject();
    } else if (button == &connect_) {
        canvas_.connectSelected();
    } else if (button == &remove_) {
        canvas_.deleteSelected();
    } else if (button == &clear_) {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Clear Patch",
            "This will remove all nodes and cables from the current patch. Continue?",
            "Clear",
            "Cancel",
            this,
            juce::ModalCallbackFunction::create([safeThis](int result) {
                if (safeThis == nullptr) {
                    return;
                }
                if (result != 0) {
                    safeThis->canvas_.clearGraph();
                }
            }));
    } else if (button == &autoConvert_) {
        const bool enabled = !canvas_.autoInsertConvertersEnabled();
        canvas_.setAutoInsertConvertersEnabled(enabled);
        autoConvert_.setButtonText(enabled ? "AutoConv: On" : "AutoConv: Off");
    } else if (button == &modeToggle_) {
        if (!inspectorApplying_) {
            applyInspectorValues();
        }
    }
}

void MainComponent::sliderValueChanged(juce::Slider* slider) {
    if (inspectorApplying_) {
        return;
    }
    if (slider == &paramA_ || slider == &paramB_) {
        applyInspectorValues();
    }
}

void MainComponent::timerCallback() {
    audioEngine_.publishGraphSnapshot();
    canvas_.syncFromGraph();
    refreshInspector();

    if (inspectedNodeType_ == "ScopeProbe" && inspectedNodeId_.has_value()) {
        const auto peak = audioEngine_.latestScopeProbePeak();
        const auto probeNode = audioEngine_.latestScopeProbeNodeId();
        if (probeNode == *inspectedNodeId_) {
            inspectorTitle_.setText("Inspector: ScopeProbe  Peak " + juce::String(peak, 3),
                                    juce::dontSendNotification);
            scopeStrip_.setTrace(audioEngine_.latestScopeProbeTrace(), true);
        } else {
            inspectorTitle_.setText("Inspector: ScopeProbe  Peak --", juce::dontSendNotification);
            scopeStrip_.setTrace({}, false);
        }
    } else {
        scopeStrip_.setTrace({}, false);
    }

    int nodeCount = 0;
    int cableCount = 0;
    std::unique_lock<std::mutex> lock(audioEngine_.graphMutex(), std::try_to_lock);
    if (lock.owns_lock()) {
        nodeCount = static_cast<int>(audioEngine_.graph().nodes().size());
        cableCount = static_cast<int>(audioEngine_.graph().connections().size());
    }

    juce::String text;
    text << "nodes: " << nodeCount
         << "  cables: " << cableCount
         << "  selected: " << canvas_.selectedCount()
         << "  auto-conv: " << (canvas_.autoInsertConvertersEnabled() ? "on" : "off")
         << "  cycle cap: " << audioEngine_.cycleConfig().activeCap()
         << "  cpu: " << juce::String(audioEngine_.lastCpuLoadPercent(), 1) << "%";

    status_.setText(text, juce::dontSendNotification);
}

void MainComponent::showNodePaletteMenu() {
    juce::PopupMenu utility;
    utility.addItem(1, "OutputStereo");
    utility.addItem(2, "Mix");
    utility.addItem(3, "UnitConvert");
    utility.addItem(4, "ScopeProbe");
    utility.addItem(5, "Constant");

    juce::PopupMenu core;
    core.addItem(10, "NeuronCore");
    core.addItem(11, "Synapse");
    core.addItem(12, "Integrator");
    core.addItem(13, "Leak");
    core.addItem(14, "Threshold");
    core.addItem(15, "Pulse");
    core.addItem(16, "Gate");
    core.addItem(17, "Slew");
    core.addItem(18, "Saturator");
    core.addItem(19, "Waveshaper");
    core.addItem(20, "AdaptiveThreshold");
    core.addItem(21, "RefractoryGate");
    core.addItem(22, "SpikeGenerator");
    core.addItem(23, "MembraneLeakCap");
    core.addItem(24, "DendriteSum");
    core.addItem(25, "DendriteNonlinearity");
    core.addItem(26, "BurstNeuron");

    juce::PopupMenu sources;
    sources.addItem(30, "Oscillator");
    sources.addItem(31, "OscillatorPhase");
    sources.addItem(32, "Noise");
    sources.addItem(33, "Drift");

    juce::PopupMenu dsp;
    dsp.addItem(40, "PhaseOps");
    dsp.addItem(41, "DelayShort");
    dsp.addItem(42, "BiquadCore");
    dsp.addItem(43, "SampleHold");
    dsp.addItem(44, "CrossfadeVCA");
    dsp.addItem(45, "Allpass");
    dsp.addItem(46, "Invert");
    dsp.addItem(47, "Modulo");
    dsp.addItem(48, "Counter");
    dsp.addItem(49, "Add");
    dsp.addItem(50, "Multiply");
    dsp.addItem(51, "Compare");
    dsp.addItem(52, "RandomGate");
    dsp.addItem(53, "Switch");
    dsp.addItem(54, "SlopeDetect");

    juce::PopupMenu menu;
    menu.addSubMenu("Utility", utility);
    menu.addSubMenu("Neuron Core", core);
    menu.addSubMenu("Sources", sources);
    menu.addSubMenu("DSP Ops", dsp);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&nodePalette_),
                       [safeThis](int choice) {
                           if (safeThis == nullptr || choice == 0) {
                               return;
                           }

                           switch (choice) {
                           case 1: safeThis->addNodeType("OutputStereo"); break;
                           case 2: safeThis->addNodeType("Mix"); break;
                           case 3: safeThis->addNodeType("UnitConvert"); break;
                           case 4: safeThis->addNodeType("ScopeProbe"); break;
                           case 5: safeThis->addNodeType("Constant"); break;
                           case 10: safeThis->addNodeType("NeuronCore"); break;
                           case 11: safeThis->addNodeType("Synapse"); break;
                           case 12: safeThis->addNodeType("Integrator"); break;
                           case 13: safeThis->addNodeType("Leak"); break;
                           case 14: safeThis->addNodeType("Threshold"); break;
                           case 15: safeThis->addNodeType("Pulse"); break;
                           case 16: safeThis->addNodeType("Gate"); break;
                           case 17: safeThis->addNodeType("Slew"); break;
                           case 18: safeThis->addNodeType("Saturator"); break;
                           case 19: safeThis->addNodeType("Waveshaper"); break;
                           case 20: safeThis->addNodeType("AdaptiveThreshold"); break;
                           case 21: safeThis->addNodeType("RefractoryGate"); break;
                           case 22: safeThis->addNodeType("SpikeGenerator"); break;
                           case 23: safeThis->addNodeType("MembraneLeakCap"); break;
                           case 24: safeThis->addNodeType("DendriteSum"); break;
                           case 25: safeThis->addNodeType("DendriteNonlinearity"); break;
                           case 26: safeThis->addNodeType("BurstNeuron"); break;
                           case 30: safeThis->addNodeType("Oscillator"); break;
                           case 31: safeThis->addNodeType("OscillatorPhase"); break;
                           case 32: safeThis->addNodeType("Noise"); break;
                           case 33: safeThis->addNodeType("Drift"); break;
                           case 40: safeThis->addNodeType("PhaseOps"); break;
                           case 41: safeThis->addNodeType("DelayShort"); break;
                           case 42: safeThis->addNodeType("BiquadCore"); break;
                           case 43: safeThis->addNodeType("SampleHold"); break;
                           case 44: safeThis->addNodeType("CrossfadeVCA"); break;
                           case 45: safeThis->addNodeType("Allpass"); break;
                           case 46: safeThis->addNodeType("Invert"); break;
                           case 47: safeThis->addNodeType("Modulo"); break;
                           case 48: safeThis->addNodeType("Counter"); break;
                           case 49: safeThis->addNodeType("Add"); break;
                           case 50: safeThis->addNodeType("Multiply"); break;
                           case 51: safeThis->addNodeType("Compare"); break;
                           case 52: safeThis->addNodeType("RandomGate"); break;
                           case 53: safeThis->addNodeType("Switch"); break;
                           case 54: safeThis->addNodeType("SlopeDetect"); break;
                           default: break;
                           }
                       });
}

void MainComponent::addNodeType(const std::string& typeName) {
    canvas_.addNode(typeName);
}

void MainComponent::saveProject() {
    juce::File startFile = currentProjectFile_.existsAsFile()
                               ? currentProjectFile_
                               : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                     .getChildFile("neural_patch.json");
    activeFileChooser_ = std::make_unique<juce::FileChooser>("Save Project", startFile, "*.json");
    juce::Component::SafePointer<MainComponent> safeThis(this);
    activeFileChooser_->launchAsync(juce::FileBrowserComponent::saveMode |
                                        juce::FileBrowserComponent::canSelectFiles |
                                        juce::FileBrowserComponent::warnAboutOverwriting,
                                    [safeThis](const juce::FileChooser& chooser) {
                                        if (safeThis == nullptr) {
                                            return;
                                        }
                                        auto outFile = chooser.getResult();
                                        safeThis->activeFileChooser_.reset();
                                        if (outFile == juce::File{}) {
                                            return;
                                        }
                                        safeThis->saveProjectToFile(outFile);
                                    });
}

void MainComponent::loadProject() {
    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Load Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");
    juce::Component::SafePointer<MainComponent> safeThis(this);
    activeFileChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                                        juce::FileBrowserComponent::canSelectFiles,
                                    [safeThis](const juce::FileChooser& chooser) {
                                        if (safeThis == nullptr) {
                                            return;
                                        }
                                        const auto inFile = chooser.getResult();
                                        safeThis->activeFileChooser_.reset();
                                        if (inFile == juce::File{}) {
                                            return;
                                        }
                                        safeThis->loadProjectFromFile(inFile);
                                    });
}

void MainComponent::saveProjectToFile(const juce::File& selectedFile) {
    auto outFile = selectedFile;
    if (outFile.getFileExtension() != ".json") {
        outFile = outFile.withFileExtension(".json");
    }

    neurons::engine::core::GraphModel graphCopy;
    {
        std::scoped_lock lock(audioEngine_.graphMutex());
        graphCopy = audioEngine_.graph();
    }
    const auto params = audioEngine_.getAllNodeParams();
    const auto layout = canvas_.exportVisualLayout();

    auto root = new juce::DynamicObject();
    root->setProperty("schema_version", 1);
    root->setProperty("app", "neural");

    juce::Array<juce::var> nodes;
    for (const auto& [id, spec] : graphCopy.nodes()) {
        auto nodeObj = new juce::DynamicObject();
        nodeObj->setProperty("id", static_cast<int>(id));
        nodeObj->setProperty("type", juce::String(spec.typeName));

        juce::Array<juce::var> inputs;
        for (const auto& in : spec.inputs) {
            auto p = new juce::DynamicObject();
            p->setProperty("index", static_cast<int>(in.index));
            p->setProperty("type", juce::String(neurons::engine::core::toString(in.type).data()));
            p->setProperty("name", juce::String(in.name));
            inputs.add(juce::var(p));
        }
        nodeObj->setProperty("inputs", inputs);

        juce::Array<juce::var> outputs;
        for (const auto& out : spec.outputs) {
            auto p = new juce::DynamicObject();
            p->setProperty("index", static_cast<int>(out.index));
            p->setProperty("type", juce::String(neurons::engine::core::toString(out.type).data()));
            p->setProperty("name", juce::String(out.name));
            outputs.add(juce::var(p));
        }
        nodeObj->setProperty("outputs", outputs);

        if (const auto it = layout.find(id); it != layout.end()) {
            nodeObj->setProperty("x", static_cast<double>(it->second.x));
            nodeObj->setProperty("y", static_cast<double>(it->second.y));
        }

        nodes.add(juce::var(nodeObj));
    }
    root->setProperty("nodes", nodes);

    juce::Array<juce::var> connections;
    for (const auto& c : graphCopy.connections()) {
        auto cObj = new juce::DynamicObject();
        cObj->setProperty("from_node", static_cast<int>(c.fromNode));
        cObj->setProperty("from_port", static_cast<int>(c.fromPort));
        cObj->setProperty("to_node", static_cast<int>(c.toNode));
        cObj->setProperty("to_port", static_cast<int>(c.toPort));
        connections.add(juce::var(cObj));
    }
    root->setProperty("connections", connections);

    juce::Array<juce::var> paramNodes;
    for (const auto& [nodeId, nodeParams] : params) {
        auto pObj = new juce::DynamicObject();
        pObj->setProperty("id", static_cast<int>(nodeId));
        auto values = new juce::DynamicObject();
        for (const auto& [key, value] : nodeParams) {
            values->setProperty(juce::String(key), static_cast<double>(value));
        }
        pObj->setProperty("values", juce::var(values));
        paramNodes.add(juce::var(pObj));
    }
    root->setProperty("params", paramNodes);

    const auto json = juce::JSON::toString(juce::var(root), false);
    if (!outFile.replaceWithText(json)) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Save Failed",
                                               "Could not write project file.");
        return;
    }

    currentProjectFile_ = outFile;
}

void MainComponent::loadProjectFromFile(const juce::File& inFile) {
    const auto parsed = juce::JSON::parse(inFile.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Load Failed",
                                               "Invalid JSON project file.");
        return;
    }

    neurons::engine::core::GraphModel newGraph;
    std::unordered_map<neurons::engine::core::NodeId, juce::Point<float>> newLayout;
    neurons::engine::rt::AudioEngine::NodeParamMap newParams;

    const auto parsePorts = [&](const juce::var& value,
                                std::vector<neurons::engine::core::PortSpec>& outPorts) -> bool {
        if (!value.isArray()) {
            return false;
        }
        const auto* arr = value.getArray();
        for (const auto& pVar : *arr) {
            auto* pObj = pVar.getDynamicObject();
            if (pObj == nullptr) {
                return false;
            }
            const auto index = pObj->getProperty("index");
            const auto type = pObj->getProperty("type");
            const auto name = pObj->getProperty("name");
            if (!index.isInt() || !type.isString() || !name.isString()) {
                return false;
            }
            auto parsedType = parseSignalType(type.toString());
            if (!parsedType.has_value()) {
                return false;
            }
            outPorts.push_back(neurons::engine::core::PortSpec{
                static_cast<neurons::engine::core::PortIndex>(static_cast<int>(index)),
                *parsedType,
                name.toString().toStdString(),
            });
        }
        return true;
    };

    const auto nodesValue = root->getProperty("nodes");
    if (!nodesValue.isArray()) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Load Failed",
                                               "Project is missing nodes.");
        return;
    }

    for (const auto& nodeVar : *nodesValue.getArray()) {
        auto* nodeObj = nodeVar.getDynamicObject();
        if (nodeObj == nullptr) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Load Failed",
                                                   "Node entry is malformed.");
            return;
        }

        const auto id = nodeObj->getProperty("id");
        const auto type = nodeObj->getProperty("type");
        if (!id.isInt() || !type.isString()) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Load Failed",
                                                   "Node id/type is invalid.");
            return;
        }

        neurons::engine::core::NodeSpec spec;
        spec.id = static_cast<neurons::engine::core::NodeId>(static_cast<int>(id));
        spec.typeName = type.toString().toStdString();
        if (!parsePorts(nodeObj->getProperty("inputs"), spec.inputs) ||
            !parsePorts(nodeObj->getProperty("outputs"), spec.outputs)) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Load Failed",
                                                   "Node ports are invalid.");
            return;
        }
        if (!newGraph.addNode(spec)) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Load Failed",
                                                   "Duplicate or invalid node ids.");
            return;
        }

        const auto x = nodeObj->getProperty("x");
        const auto y = nodeObj->getProperty("y");
        if ((x.isDouble() || x.isInt()) && (y.isDouble() || y.isInt())) {
            newLayout.emplace(spec.id,
                              juce::Point<float>(static_cast<float>(static_cast<double>(x)),
                                                 static_cast<float>(static_cast<double>(y))));
        }
    }

    const auto connectionsValue = root->getProperty("connections");
    if (connectionsValue.isArray()) {
        for (const auto& cVar : *connectionsValue.getArray()) {
            auto* cObj = cVar.getDynamicObject();
            if (cObj == nullptr) {
                continue;
            }
            const auto fromNode = cObj->getProperty("from_node");
            const auto fromPort = cObj->getProperty("from_port");
            const auto toNode = cObj->getProperty("to_node");
            const auto toPort = cObj->getProperty("to_port");
            if (!fromNode.isInt() || !fromPort.isInt() || !toNode.isInt() || !toPort.isInt()) {
                continue;
            }
            const auto result = newGraph.addConnection({
                static_cast<neurons::engine::core::NodeId>(static_cast<int>(fromNode)),
                static_cast<neurons::engine::core::PortIndex>(static_cast<int>(fromPort)),
                static_cast<neurons::engine::core::NodeId>(static_cast<int>(toNode)),
                static_cast<neurons::engine::core::PortIndex>(static_cast<int>(toPort)),
            });
            if (!result.ok) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Load Failed",
                                                       "A connection in this project is invalid.");
                return;
            }
        }
    }

    const auto paramsValue = root->getProperty("params");
    if (paramsValue.isArray()) {
        for (const auto& pNodeVar : *paramsValue.getArray()) {
            auto* pNodeObj = pNodeVar.getDynamicObject();
            if (pNodeObj == nullptr) {
                continue;
            }
            const auto id = pNodeObj->getProperty("id");
            const auto values = pNodeObj->getProperty("values");
            if (!id.isInt()) {
                continue;
            }
            auto* valuesObj = values.getDynamicObject();
            if (valuesObj == nullptr) {
                continue;
            }
            auto nodeId = static_cast<neurons::engine::core::NodeId>(static_cast<int>(id));
            for (const auto& kv : valuesObj->getProperties()) {
                const auto& key = kv.name.toString().toStdString();
                const auto& value = kv.value;
                if (value.isDouble() || value.isInt()) {
                    newParams[nodeId][key] = static_cast<float>(static_cast<double>(value));
                }
            }
        }
    }

    audioEngine_.replaceGraphAndParams(newGraph, newParams);
    audioEngine_.publishGraphSnapshot();
    canvas_.syncFromGraph();
    canvas_.applyVisualLayout(newLayout);
    refreshInspector();
    currentProjectFile_ = inFile;
}

void MainComponent::refreshInspector() {
    const auto selectedId = canvas_.singleSelectedNodeId();
    const auto selectedType = canvas_.singleSelectedNodeType();

    if (!selectedId.has_value() || !selectedType.has_value()) {
        inspectedNodeId_.reset();
        inspectedNodeType_.clear();
        inspectorTitle_.setText("Inspector: no node selected", juce::dontSendNotification);
        paramA_.setVisible(false);
        paramB_.setVisible(false);
        paramALabel_.setVisible(false);
        paramBLabel_.setVisible(false);
        modeLabel_.setVisible(false);
        modeToggle_.setVisible(false);
        scopeLabel_.setVisible(false);
        scopeStrip_.setVisible(false);
        return;
    }

    if (inspectedNodeId_ == selectedId && inspectedNodeType_ == *selectedType) {
        return;
    }

    inspectedNodeId_ = selectedId;
    inspectedNodeType_ = *selectedType;

    inspectorApplying_ = true;

    paramA_.setVisible(true);
    paramALabel_.setVisible(true);
    paramB_.setVisible(false);
    paramBLabel_.setVisible(false);
    modeLabel_.setVisible(false);
    modeToggle_.setVisible(false);
    scopeLabel_.setVisible(false);
    scopeStrip_.setVisible(false);
    paramAKey_.clear();
    paramBKey_.clear();

    if (inspectedNodeType_ == "Oscillator") {
        inspectorTitle_.setText("Inspector: Oscillator", juce::dontSendNotification);
        paramAKey_ = "freq_hz";
        paramALabel_.setText("Frequency (Hz)", juce::dontSendNotification);
        paramA_.setRange(20.0, 2000.0, 0.01);
        paramA_.setSkewFactorFromMidPoint(220.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(220.0f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "Saturator") {
        inspectorTitle_.setText("Inspector: Saturator", juce::dontSendNotification);
        paramAKey_ = "drive";
        paramALabel_.setText("Drive", juce::dontSendNotification);
        paramA_.setRange(0.2, 8.0, 0.001);
        paramA_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "Mix") {
        inspectorTitle_.setText("Inspector: Mix", juce::dontSendNotification);
        paramAKey_ = "inlets";
        paramALabel_.setText("Inlets", juce::dontSendNotification);
        paramA_.setRange(2.0, 8.0, 1.0);
        paramA_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(2.0f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "NeuronCore") {
        inspectorTitle_.setText("Inspector: NeuronCore", juce::dontSendNotification);
        paramAKey_ = "gain";
        paramBKey_ = "tau_ms";
        paramALabel_.setText("Gain", juce::dontSendNotification);
        paramBLabel_.setText("Tau (ms)", juce::dontSendNotification);
        paramA_.setRange(0.0, 4.0, 0.001);
        paramA_.setSkewFactor(1.0);
        paramB_.setRange(1.0, 200.0, 0.01);
        paramB_.setSkewFactorFromMidPoint(20.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(20.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "BiquadCore") {
        inspectorTitle_.setText("Inspector: BiquadCore", juce::dontSendNotification);
        paramAKey_ = "cutoff_hz";
        paramALabel_.setText("Cutoff (Hz)", juce::dontSendNotification);
        paramA_.setRange(20.0, 18000.0, 0.1);
        paramA_.setSkewFactorFromMidPoint(1200.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1200.0f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "DelayShort") {
        inspectorTitle_.setText("Inspector: DelayShort", juce::dontSendNotification);
        paramAKey_ = "delay_ms";
        paramALabel_.setText("Delay (ms)", juce::dontSendNotification);
        paramA_.setRange(0.1, 40.0, 0.01);
        paramA_.setSkewFactorFromMidPoint(4.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.33f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "Waveshaper") {
        inspectorTitle_.setText("Inspector: Waveshaper", juce::dontSendNotification);
        paramAKey_ = "drive";
        paramBKey_ = "curve";
        paramALabel_.setText("Drive", juce::dontSendNotification);
        paramBLabel_.setText("Curve Blend", juce::dontSendNotification);
        paramA_.setRange(0.1, 8.0, 0.001);
        paramA_.setSkewFactorFromMidPoint(1.0);
        paramB_.setRange(0.0, 1.0, 0.001);
        paramB_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(0.5f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "Allpass") {
        inspectorTitle_.setText("Inspector: Allpass", juce::dontSendNotification);
        paramAKey_ = "delay_ms";
        paramBKey_ = "feedback";
        paramALabel_.setText("Delay (ms)", juce::dontSendNotification);
        paramBLabel_.setText("Feedback", juce::dontSendNotification);
        paramA_.setRange(0.1, 40.0, 0.01);
        paramA_.setSkewFactorFromMidPoint(6.0);
        paramB_.setRange(-0.99, 0.99, 0.001);
        paramB_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(6.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(0.6f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "Modulo") {
        inspectorTitle_.setText("Inspector: Modulo", juce::dontSendNotification);
        paramAKey_ = "modulus";
        paramALabel_.setText("Modulus", juce::dontSendNotification);
        paramA_.setRange(0.001, 64.0, 0.001);
        paramA_.setSkewFactorFromMidPoint(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "Counter") {
        inspectorTitle_.setText("Inspector: Counter", juce::dontSendNotification);
        paramAKey_ = "min";
        paramBKey_ = "max";
        paramALabel_.setText("Min", juce::dontSendNotification);
        paramBLabel_.setText("Max", juce::dontSendNotification);
        paramA_.setRange(-128.0, 127.0, 1.0);
        paramB_.setRange(-128.0, 127.0, 1.0);
        paramA_.setSkewFactor(1.0);
        paramB_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(0.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(15.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
        modeLabel_.setText("Mode", juce::dontSendNotification);
        modeLabel_.setVisible(true);
        modeToggle_.setButtonText("Wrap");
        modeToggle_.setToggleState(audioEngine_.getNodeParam(*inspectedNodeId_, "wrap").value_or(1.0f) >= 0.5f,
                                   juce::dontSendNotification);
        modeToggle_.setVisible(true);
    } else if (inspectedNodeType_ == "Constant") {
        inspectorTitle_.setText("Inspector: Constant", juce::dontSendNotification);
        paramAKey_ = "value";
        paramALabel_.setText("Value", juce::dontSendNotification);
        paramA_.setRange(-128.0, 127.0, 0.001);
        paramA_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(0.0f), juce::dontSendNotification);
    } else if (inspectedNodeType_ == "RandomGate") {
        inspectorTitle_.setText("Inspector: RandomGate", juce::dontSendNotification);
        paramAKey_ = "prob";
        paramBKey_ = "pulse_ms";
        paramALabel_.setText("Probability", juce::dontSendNotification);
        paramBLabel_.setText("Pulse (ms)", juce::dontSendNotification);
        paramA_.setRange(0.0, 1.0, 0.001);
        paramB_.setRange(0.02, 20.0, 0.01);
        paramA_.setSkewFactor(1.0);
        paramB_.setSkewFactorFromMidPoint(2.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(0.5f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(2.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "Compare") {
        inspectorTitle_.setText("Inspector: Compare", juce::dontSendNotification);
        paramA_.setVisible(false);
        paramB_.setVisible(false);
        paramALabel_.setVisible(false);
        paramBLabel_.setVisible(false);
        modeLabel_.setText("Mode", juce::dontSendNotification);
        modeLabel_.setVisible(true);
        modeToggle_.setButtonText("A > B");
        modeToggle_.setToggleState(audioEngine_.getNodeParam(*inspectedNodeId_, "greater").value_or(1.0f) >= 0.5f,
                                   juce::dontSendNotification);
        modeToggle_.setVisible(true);
    } else if (inspectedNodeType_ == "Switch") {
        inspectorTitle_.setText("Inspector: Switch", juce::dontSendNotification);
        paramA_.setVisible(false);
        paramB_.setVisible(false);
        paramALabel_.setVisible(false);
        paramBLabel_.setVisible(false);
        modeLabel_.setText("Default", juce::dontSendNotification);
        modeLabel_.setVisible(true);
        modeToggle_.setButtonText("Use B (no select cable)");
        modeToggle_.setToggleState(audioEngine_.getNodeParam(*inspectedNodeId_, "select_b").value_or(0.0f) >= 0.5f,
                                   juce::dontSendNotification);
        modeToggle_.setVisible(true);
    } else if (inspectedNodeType_ == "SlopeDetect") {
        inspectorTitle_.setText("Inspector: SlopeDetect", juce::dontSendNotification);
        paramAKey_ = "threshold";
        paramALabel_.setText("Deadband", juce::dontSendNotification);
        paramA_.setRange(1.0e-7, 0.1, 1.0e-7);
        paramA_.setSkewFactorFromMidPoint(1.0e-4);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0e-4f),
                         juce::dontSendNotification);
    } else if (inspectedNodeType_ == "ScopeProbe") {
        inspectorTitle_.setText("Inspector: ScopeProbe", juce::dontSendNotification);
        paramA_.setVisible(false);
        paramB_.setVisible(false);
        paramALabel_.setVisible(false);
        paramBLabel_.setVisible(false);
        scopeLabel_.setVisible(true);
        scopeStrip_.setVisible(true);
    } else if (inspectedNodeType_ == "AdaptiveThreshold") {
        inspectorTitle_.setText("Inspector: AdaptiveThreshold", juce::dontSendNotification);
        paramAKey_ = "base_threshold";
        paramBKey_ = "adapt";
        paramALabel_.setText("Base Threshold", juce::dontSendNotification);
        paramBLabel_.setText("Adapt Amount", juce::dontSendNotification);
        paramA_.setRange(-1.0, 1.0, 0.001);
        paramB_.setRange(0.0, 2.0, 0.001);
        paramA_.setSkewFactor(1.0);
        paramB_.setSkewFactorFromMidPoint(0.2);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(0.5f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(0.25f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "RefractoryGate") {
        inspectorTitle_.setText("Inspector: RefractoryGate", juce::dontSendNotification);
        paramAKey_ = "refractory_ms";
        paramBKey_ = "pulse_ms";
        paramALabel_.setText("Refractory (ms)", juce::dontSendNotification);
        paramBLabel_.setText("Pulse (ms)", juce::dontSendNotification);
        paramA_.setRange(0.0, 500.0, 0.01);
        paramB_.setRange(0.02, 20.0, 0.01);
        paramA_.setSkewFactorFromMidPoint(30.0);
        paramB_.setSkewFactorFromMidPoint(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(30.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "SpikeGenerator") {
        inspectorTitle_.setText("Inspector: SpikeGenerator", juce::dontSendNotification);
        paramAKey_ = "threshold";
        paramBKey_ = "pulse_ms";
        paramALabel_.setText("Threshold", juce::dontSendNotification);
        paramBLabel_.setText("Pulse (ms)", juce::dontSendNotification);
        paramA_.setRange(-1.0, 1.0, 0.001);
        paramB_.setRange(0.02, 20.0, 0.01);
        paramA_.setSkewFactor(1.0);
        paramB_.setSkewFactorFromMidPoint(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(0.5f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "MembraneLeakCap") {
        inspectorTitle_.setText("Inspector: MembraneLeakCap", juce::dontSendNotification);
        paramAKey_ = "tau_ms";
        paramBKey_ = "leak";
        paramALabel_.setText("Tau (ms)", juce::dontSendNotification);
        paramBLabel_.setText("Leak", juce::dontSendNotification);
        paramA_.setRange(0.1, 500.0, 0.01);
        paramB_.setRange(0.0, 1.0, 0.001);
        paramA_.setSkewFactorFromMidPoint(20.0);
        paramB_.setSkewFactorFromMidPoint(0.05);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(20.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(0.01f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "DendriteSum") {
        inspectorTitle_.setText("Inspector: DendriteSum", juce::dontSendNotification);
        paramAKey_ = "gain_a";
        paramBKey_ = "gain_b";
        paramALabel_.setText("Gain A", juce::dontSendNotification);
        paramBLabel_.setText("Gain B", juce::dontSendNotification);
        paramA_.setRange(-8.0, 8.0, 0.001);
        paramB_.setRange(-8.0, 8.0, 0.001);
        paramA_.setSkewFactor(1.0);
        paramB_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "DendriteNonlinearity") {
        inspectorTitle_.setText("Inspector: DendriteNonlinearity", juce::dontSendNotification);
        paramAKey_ = "drive";
        paramBKey_ = "bias";
        paramALabel_.setText("Drive", juce::dontSendNotification);
        paramBLabel_.setText("Bias", juce::dontSendNotification);
        paramA_.setRange(0.01, 20.0, 0.001);
        paramB_.setRange(-4.0, 4.0, 0.001);
        paramA_.setSkewFactorFromMidPoint(1.0);
        paramB_.setSkewFactor(1.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(1.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(0.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else if (inspectedNodeType_ == "BurstNeuron") {
        inspectorTitle_.setText("Inspector: BurstNeuron", juce::dontSendNotification);
        paramAKey_ = "count";
        paramBKey_ = "interval_ms";
        paramALabel_.setText("Count", juce::dontSendNotification);
        paramBLabel_.setText("Interval (ms)", juce::dontSendNotification);
        paramA_.setRange(1.0, 32.0, 1.0);
        paramB_.setRange(0.02, 200.0, 0.01);
        paramA_.setSkewFactor(1.0);
        paramB_.setSkewFactorFromMidPoint(8.0);
        paramA_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramAKey_).value_or(3.0f), juce::dontSendNotification);
        paramB_.setValue(audioEngine_.getNodeParam(*inspectedNodeId_, paramBKey_).value_or(8.0f), juce::dontSendNotification);
        paramB_.setVisible(true);
        paramBLabel_.setVisible(true);
    } else {
        inspectorTitle_.setText("Inspector: no editable params", juce::dontSendNotification);
        paramA_.setVisible(false);
        paramB_.setVisible(false);
        paramALabel_.setVisible(false);
        paramBLabel_.setVisible(false);
        modeLabel_.setVisible(false);
        modeToggle_.setVisible(false);
    }

    inspectorApplying_ = false;
}

void MainComponent::applyInspectorValues() {
    if (!inspectedNodeId_.has_value()) {
        return;
    }

    if (inspectedNodeType_ == "Mix" && paramAKey_ == "inlets" && paramA_.isVisible()) {
        const int inletCount = juce::jlimit(2, 8, juce::roundToInt(static_cast<float>(paramA_.getValue())));
        {
            std::scoped_lock lock(audioEngine_.graphMutex());
            auto& graph = audioEngine_.graph();
            if (const auto* node = graph.getNode(*inspectedNodeId_); node != nullptr && node->typeName == "Mix") {
                using neurons::engine::core::PortSpec;
                using neurons::engine::core::SignalType;
                std::vector<PortSpec> inputs;
                inputs.reserve(static_cast<std::size_t>(inletCount));
                for (int i = 0; i < inletCount; ++i) {
                    inputs.push_back(PortSpec{
                        static_cast<neurons::engine::core::PortIndex>(i),
                        SignalType::BipolarAudio,
                        "in" + std::to_string(i + 1),
                    });
                }
                graph.replaceNodeInputs(*inspectedNodeId_, std::move(inputs));
            }
        }
        audioEngine_.setNodeParam(*inspectedNodeId_, "inlets", static_cast<float>(inletCount));
        audioEngine_.publishGraphSnapshot();
        canvas_.syncFromGraph();
        return;
    }

    if (!paramAKey_.empty() && paramA_.isVisible()) {
        audioEngine_.setNodeParam(*inspectedNodeId_, paramAKey_, static_cast<float>(paramA_.getValue()));
    }
    if (!paramBKey_.empty() && paramB_.isVisible()) {
        audioEngine_.setNodeParam(*inspectedNodeId_, paramBKey_, static_cast<float>(paramB_.getValue()));
    }
    if (inspectedNodeType_ == "Counter" && modeToggle_.isVisible()) {
        audioEngine_.setNodeParam(*inspectedNodeId_, "wrap", modeToggle_.getToggleState() ? 1.0f : 0.0f);
    } else if (inspectedNodeType_ == "Compare" && modeToggle_.isVisible()) {
        audioEngine_.setNodeParam(*inspectedNodeId_, "greater", modeToggle_.getToggleState() ? 1.0f : 0.0f);
    } else if (inspectedNodeType_ == "Switch" && modeToggle_.isVisible()) {
        audioEngine_.setNodeParam(*inspectedNodeId_, "select_b", modeToggle_.getToggleState() ? 1.0f : 0.0f);
    }
    audioEngine_.publishGraphSnapshot();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    audioEngine_.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    juce::ScopedNoDenormals noDenormals;

    if (bufferToFill.buffer == nullptr || bufferToFill.numSamples <= 0) {
        return;
    }

    auto* buffer = bufferToFill.buffer;
    const int start = bufferToFill.startSample;
    const int numSamples = bufferToFill.numSamples;

    const int numChannels = buffer->getNumChannels();
    if (numChannels <= 0) {
        return;
    }

    auto* left = buffer->getWritePointer(0, start);
    float* right = nullptr;

    if (numChannels > 1) {
        right = buffer->getWritePointer(1, start);
    } else {
        monoScratch_.assign(static_cast<std::size_t>(numSamples), 0.0f);
        right = monoScratch_.data();
    }

    audioEngine_.processBlock(left, right, numSamples);

    for (int ch = 2; ch < numChannels; ++ch) {
        buffer->clear(ch, start, numSamples);
    }
}

void MainComponent::releaseResources() {
    audioEngine_.releaseResources();
}

} // namespace neurons::app
