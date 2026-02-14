#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <atomic>

#include "../engine/rt/AudioEngine.h"
#include "PatchCanvas.h"

namespace neurons::app {

class ScopeStrip final : public juce::Component {
public:
    void setTrace(std::vector<float> trace, bool active);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> trace_;
    bool active_{false};
};

class SpectrogramStrip final : public juce::Component {
public:
    void setTrace(std::vector<float> trace, bool active);
    void paint(juce::Graphics& g) override;

private:
    std::vector<std::vector<float>> columns_;
    bool active_{false};
};

class PhaseStrip final : public juce::Component {
public:
    void setTrace(std::vector<float> trace, bool active);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> trace_;
    bool active_{false};
};

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Button::Listener,
                            private juce::Slider::Listener,
                            private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void timerCallback() override;
    void showNodePaletteMenu();
    void addNodeType(const std::string& typeName);
    void saveProject();
    void loadProject();
    void saveProjectToFile(const juce::File& outFile);
    void loadProjectFromFile(const juce::File& inFile);
    void chooseRecordFolder();
    void chooseSampleWav();
    void editBytebeatScript();
    void startOutputRecording();
    void stopOutputRecording();
    void refreshInspector();
    void applyInspectorValues();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    neurons::engine::rt::AudioEngine audioEngine_;
    PatchCanvas canvas_;

    juce::TextButton nodePalette_{"+ Node"};
    juce::TextButton load_{"Load"};
    juce::TextButton save_{"Save"};
    juce::TextButton connect_{"Connect Sel"};
    juce::TextButton clear_{"Clear"};
    juce::TextButton autoConvert_{"AutoConv: On"};

    juce::Label status_;
    juce::Label inspectorTitle_{"inspector", "Inspector"};
    juce::Label paramALabel_{"param_a", "Param A"};
    juce::Label paramBLabel_{"param_b", "Param B"};
    juce::Label modeLabel_{"mode", "Mode"};
    juce::Slider paramA_;
    juce::Slider paramB_;
    juce::ToggleButton modeToggle_{"Wrap"};
    juce::TextButton recordFolderButton_{"Set Record Folder"};
    juce::Label recordFolderLabel_{"record_folder", ""};
    juce::TextButton sampleLoadButton_{"Load WAV"};
    juce::Label sampleFileLabel_{"sample_file", ""};
    juce::TextButton bytebeatEditButton_{"Load JS"};
    juce::Label bytebeatLabel_{"bytebeat_expr", ""};
    juce::Label scopeLabel_{"scope", "Scope"};
    ScopeStrip scopeStrip_;
    juce::Label spectrogramLabel_{"spectrogram", "Spectrogram"};
    SpectrogramStrip spectrogramStrip_;
    juce::Label phaseLabel_{"phase", "Phase"};
    PhaseStrip phaseStrip_;

    std::optional<neurons::engine::core::NodeId> inspectedNodeId_;
    std::string inspectedNodeType_;
    std::string paramAKey_;
    std::string paramBKey_;
    bool inspectorApplying_{false};
    juce::File currentProjectFile_;
    juce::File outputRecordFolder_;
    std::unique_ptr<juce::FileChooser> activeFileChooser_;
    juce::TimeSliceThread recordThread_{"Output Recorder"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter_;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter_{nullptr};

    std::vector<float> monoScratch_;
};

} // namespace neurons::app
